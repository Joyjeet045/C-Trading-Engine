#include "OrderBook.h"
#include <algorithm>
#include <iostream>

OrderBook::OrderBook(const std::string& _symbol) :  last_trade_price(0.0),symbol(_symbol) {}

void OrderBook::add_order(std::shared_ptr<Order> order) {
    std::lock_guard<std::mutex> lock(book_mutex);
    
    if (order->type == OrderType::STOP_LOSS || order->type == OrderType::STOP_LIMIT || order->type == OrderType::TRAILING_STOP) {
        if (should_trigger_stop_loss(order)) {
            execute_stop_loss_order(order, "immediately");
            return; 
        }
        stop_loss_orders.push_back(order);
        return;
    }
    
    if (order->side == OrderSide::BUY) {
        buy_orders[order->price].push_back(order);
    } else {
        sell_orders[order->price].push_back(order);
    }
}

void OrderBook::cancel_order(uint64_t order_id) {
    std::lock_guard<std::mutex> lock(book_mutex);
    
    for (auto& [price, orders] : buy_orders) {
        auto it = std::find_if(orders.begin(), orders.end(),
            [order_id](const auto& order) { return order->id == order_id; });
        if (it != orders.end()) {
            (*it)->status = OrderStatus::CANCELLED;
            orders.erase(it);
            if (orders.empty()) buy_orders.erase(price);
            return;
        }
    }
    
    for (auto& [price, orders] : sell_orders) {
        auto it = std::find_if(orders.begin(), orders.end(),
            [order_id](const auto& order) { return order->id == order_id; });
        if (it != orders.end()) {
            (*it)->status = OrderStatus::CANCELLED;
            orders.erase(it);
            if (orders.empty()) sell_orders.erase(price);
            return;
        }
    }
    
    auto it = std::find_if(stop_loss_orders.begin(), stop_loss_orders.end(),
        [order_id](const auto& order) { return order->id == order_id; });
    if (it != stop_loss_orders.end()) {
        (*it)->status = OrderStatus::CANCELLED;
        stop_loss_orders.erase(it);
    }
}

std::vector<std::shared_ptr<Order>> OrderBook::match_orders() {
    std::lock_guard<std::mutex> lock(book_mutex);
    std::vector<std::shared_ptr<Order>> matched_orders;
    
    while (!buy_orders.empty() && !sell_orders.empty()) {
        auto best_buy = buy_orders.rbegin(); 
        auto best_sell = sell_orders.begin(); 
        
        if (best_buy->first < best_sell->first) break;
        
        auto buy_order = best_buy->second.front();
        auto sell_order = best_sell->second.front();
        
        if (buy_order->client_id == sell_order->client_id) {
            if (buy_order->timestamp < sell_order->timestamp) {
                best_buy->second.erase(best_buy->second.begin());
                if (best_buy->second.empty()) {
                    buy_orders.erase(std::prev(best_buy.base()));
                }
            } else {
                best_sell->second.erase(best_sell->second.begin());
                if (best_sell->second.empty()) {
                    sell_orders.erase(best_sell);
                }
            }
            continue;
        }
        
        if (execute_trade(buy_order, sell_order)) {
            matched_orders.push_back(buy_order);
            matched_orders.push_back(sell_order);
        }
        
        if (buy_order->filled_quantity >= buy_order->quantity) {
            best_buy->second.erase(best_buy->second.begin());
            if (best_buy->second.empty()) {
                buy_orders.erase(std::prev(best_buy.base()));
            }
        }
        
        if (sell_order->filled_quantity >= sell_order->quantity) {
            best_sell->second.erase(best_sell->second.begin());
            if (best_sell->second.empty()) {
                sell_orders.erase(best_sell);
            }
        }
    }
    
    return matched_orders;
}

void OrderBook::check_stop_loss_orders() {
    std::lock_guard<std::mutex> lock(book_mutex);
    if (last_trade_price <= 0.0) {
        return;
    }
    
    for (auto it = stop_loss_orders.begin(); it != stop_loss_orders.end();) {
        auto order = *it;
        
        // Update trailing stop prices if needed
        if (order->type == OrderType::TRAILING_STOP) {
            update_trailing_stop_price(order);
        }
        
        if (should_trigger_stop_loss(order)) {
            execute_stop_loss_order(order, "due to price movement");
            it = stop_loss_orders.erase(it);
        } else {
            ++it;
        }
    }
}

double OrderBook::get_best_bid() const {
    std::lock_guard<std::mutex> lock(book_mutex);
    return buy_orders.empty() ? 0.0 : buy_orders.rbegin()->first;
}

double OrderBook::get_best_ask() const {
    std::lock_guard<std::mutex> lock(book_mutex);
    return sell_orders.empty() ? 0.0 : sell_orders.begin()->first;
}

double OrderBook::get_last_price() const {
    std::lock_guard<std::mutex> lock(book_mutex);
    return last_trade_price;
}

void OrderBook::set_trade_callback(TradeCallback callback) {
    std::lock_guard<std::mutex> lock(book_mutex);
    trade_callback = callback;
}

bool OrderBook::execute_trade(std::shared_ptr<Order> buy_order, std::shared_ptr<Order> sell_order) {
    double trade_quantity = std::min(buy_order->quantity - buy_order->filled_quantity,
                                   sell_order->quantity - sell_order->filled_quantity);
    
    if (trade_quantity <= 0) return false;
    
    double trade_price;
    if (buy_order->type == OrderType::MARKET) {
        trade_price = sell_order->price; 
    } else if (sell_order->type == OrderType::MARKET) {
        trade_price = buy_order->price;   
    } else {
        if (buy_order->timestamp < sell_order->timestamp) {
            trade_price = buy_order->price; 
        } else {
            trade_price = sell_order->price;
        }
    } 
    
    
    buy_order->filled_quantity += trade_quantity;
    sell_order->filled_quantity += trade_quantity;
    
    if (buy_order->filled_quantity >= buy_order->quantity) {
        buy_order->status = OrderStatus::FILLED;
    }
    if (sell_order->filled_quantity >= sell_order->quantity) {
        sell_order->status = OrderStatus::FILLED;
    }
    
    last_trade_price = trade_price;
    
    // Notify VWAP calculator about the trade
    if (trade_callback) {
        trade_callback(symbol, trade_price, trade_quantity);
    }
    
    std::cout << "Trade executed: " << trade_quantity << " @ " << trade_price 
              << " between " << buy_order->client_id << " and " << sell_order->client_id << std::endl;
    
    return true;
}

double OrderBook::execute_market_order(std::shared_ptr<Order> market_order, OrderSide opposite_side, double max_quantity) {
    std::lock_guard<std::mutex> lock(book_mutex);
    return execute_market_order_internal(market_order, opposite_side, max_quantity);
}

double OrderBook::execute_market_order_internal(std::shared_ptr<Order> market_order, OrderSide opposite_side, double max_quantity) {
    // Note: This function assumes the caller already holds the book_mutex
    
    double total_executed = 0.0;
    auto& opposite_orders = (opposite_side == OrderSide::BUY) ? buy_orders : sell_orders;
    
    if (opposite_side == OrderSide::BUY) {
        while (total_executed < max_quantity && !opposite_orders.empty()) {
            auto best_opposite = opposite_orders.rbegin();
            if (best_opposite->second.empty()) {
                opposite_orders.erase(std::prev(best_opposite.base()));
                continue;
            }
            auto opposite_order = best_opposite->second.front();
            if (opposite_order->client_id == market_order->client_id) {
                best_opposite->second.erase(best_opposite->second.begin());
                if (best_opposite->second.empty()) {
                    opposite_orders.erase(std::prev(best_opposite.base()));
                }
                continue;
            }
            double available_quantity = opposite_order->quantity - opposite_order->filled_quantity;
            double market_remaining = max_quantity - total_executed;
            double trade_quantity = std::min(available_quantity, market_remaining);
            if (trade_quantity <= 0) break;
            execute_trade(opposite_order, market_order);
            total_executed += trade_quantity;
            if (opposite_order->filled_quantity >= opposite_order->quantity) {
                best_opposite->second.erase(best_opposite->second.begin());
                if (best_opposite->second.empty()) {
                    opposite_orders.erase(std::prev(best_opposite.base()));
                }
            }
        }
    } else {
        while (total_executed < max_quantity && !opposite_orders.empty()) {
            auto best_opposite = opposite_orders.begin();
            if (best_opposite->second.empty()) {
                opposite_orders.erase(best_opposite);
                continue;
            }
            auto opposite_order = best_opposite->second.front();
            if (opposite_order->client_id == market_order->client_id) {
                best_opposite->second.erase(best_opposite->second.begin());
                if (best_opposite->second.empty()) {
                    opposite_orders.erase(best_opposite);
                }
                continue;
            }
            double available_quantity = opposite_order->quantity - opposite_order->filled_quantity;
            double market_remaining = max_quantity - total_executed;
            double trade_quantity = std::min(available_quantity, market_remaining);
            if (trade_quantity <= 0) break;
            execute_trade(market_order, opposite_order);
            total_executed += trade_quantity;
            if (opposite_order->filled_quantity >= opposite_order->quantity) {
                best_opposite->second.erase(best_opposite->second.begin());
                if (best_opposite->second.empty()) {
                    opposite_orders.erase(best_opposite);
                }
            }
        }
    }
    return total_executed;
}

bool OrderBook::should_trigger_stop_loss(std::shared_ptr<Order> order) const {
    if (last_trade_price <= 0.0) {
        return false;
    }
    
    if (order->type == OrderType::TRAILING_STOP) {
        // For TRAILING_STOP, check if current price has moved against the trailing stop
        if (order->side == OrderSide::SELL) {
            return last_trade_price <= order->price;
        } else if (order->side == OrderSide::BUY) {
            return last_trade_price >= order->price;
        }
    } else {
        // For STOP_LOSS and STOP_LIMIT, use the original logic
        if (order->side == OrderSide::SELL && last_trade_price <= order->price) {
            return true;
        } else if (order->side == OrderSide::BUY && last_trade_price >= order->price) {
            return true;
        }
    }
    
    return false;
}

void OrderBook::execute_stop_loss_order(std::shared_ptr<Order> order, const std::string& trigger_context) {
    std::cout << "Stop order " << order->id << " triggered " << trigger_context << " at price " << last_trade_price << std::endl;
    
    double executed_quantity = 0.0;
    
    if (order->type == OrderType::STOP_LOSS) {
        // Convert to market order and execute immediately
        order->type = OrderType::MARKET;
        executed_quantity = execute_market_order_internal(order, 
            (order->side == OrderSide::BUY) ? OrderSide::SELL : OrderSide::BUY, 
            order->quantity);
    } else if (order->type == OrderType::STOP_LIMIT) {
        // Convert to limit order and add to order book
        order->type = OrderType::LIMIT;
        order->price = order->limit_price; // Use the limit price for the limit order
        
        if (order->side == OrderSide::BUY) {
            buy_orders[order->price].push_back(order);
        } else {
            sell_orders[order->price].push_back(order);
        }
        
        std::cout << "Stop limit order " << order->id << " converted to limit order at price " << order->price << std::endl;
        return; // Don't set status yet, let normal matching handle it
    } else if (order->type == OrderType::TRAILING_STOP) {
        // Convert to market order and execute immediately
        order->type = OrderType::MARKET;
        executed_quantity = execute_market_order_internal(order, 
            (order->side == OrderSide::BUY) ? OrderSide::SELL : OrderSide::BUY, 
            order->quantity);
    }
    
    if (executed_quantity == order->quantity) {
        order->status = OrderStatus::FILLED;
        std::cout << "Stop loss order " << order->id << " fully executed: " << executed_quantity << " shares" << std::endl;
    } else if (executed_quantity > 0) {
        order->status = OrderStatus::PARTIAL_FILLED;
        std::cout << "Stop loss order " << order->id << " partially executed: " << executed_quantity << "/" << order->quantity << " shares" << std::endl;
        std::cout << "Remaining " << (order->quantity - executed_quantity) << " shares rejected - no liquidity" << std::endl;
    } else {
        order->status = OrderStatus::REJECTED;
        std::cout << "Stop loss order " << order->id << " rejected: no liquidity available" << std::endl;
    }
}

void OrderBook::update_trailing_stop_price(std::shared_ptr<Order> order) {
    if (order->type != OrderType::TRAILING_STOP) {
        return;
    }
    
    if (order->side == OrderSide::SELL) {
        // For SELL trailing stops, track the highest price and set stop below it
        if (last_trade_price > order->highest_price) {
            order->highest_price = last_trade_price;
            order->price = last_trade_price - order->trailing_amount;
            std::cout << "Trailing stop " << order->id << " updated: highest=" << order->highest_price 
                      << ", stop=" << order->price << std::endl;
        }
    } else if (order->side == OrderSide::BUY) {
        // For BUY trailing stops, track the lowest price and set stop above it
        if (last_trade_price < order->lowest_price || order->lowest_price == 0.0) {
            order->lowest_price = last_trade_price;
            order->price = last_trade_price + order->trailing_amount;
            std::cout << "Trailing stop " << order->id << " updated: lowest=" << order->lowest_price 
                      << ", stop=" << order->price << std::endl;
        }
    }
}
