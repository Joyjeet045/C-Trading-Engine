#include "MatchingEngine.h"
#include <iostream>
#include <algorithm>
#include <thread>
#include <chrono>

MatchingEngine::MatchingEngine() : next_order_id(1) {}

uint64_t MatchingEngine::submit_order(const std::string& symbol, OrderType type, OrderSide side,
                                     double price, double quantity, const std::string& client_id) {
    if (!validate_order(symbol, type, side, price, quantity, client_id)) {
        return 0;
    }
    
    uint64_t order_id = next_order_id++;
    
    std::lock_guard<std::mutex> lock(engine_mutex);
    
    if (order_books.find(symbol) == order_books.end()) {
        order_books[symbol] = std::make_shared<OrderBook>(symbol);
        order_books[symbol]->set_trade_callback([this](const std::string& sym, double price, double volume) {
            feed_trade_to_vwap_calculator(sym, price, volume);
        });
    }
    
    auto order = std::make_shared<Order>(order_id, symbol, type, side, price, quantity, client_id);
    
    if (type == OrderType::MARKET) {
        if (side == OrderSide::BUY) {
            execute_market_buy_order(order_books[symbol], order);
        } else {
            execute_market_sell_order(order_books[symbol], order);
        }
    } else {
        order_books[symbol]->add_order(order);
        thread_pool.enqueue([this, symbol]() {
            process_matching(symbol);
        });
    }
    
    client_orders[client_id].push_back(order_id);
    return order_id;
}

uint64_t MatchingEngine::submit_stop_limit_order(const std::string& symbol, OrderSide side,
                                                double stop_price, double limit_price, double quantity, 
                                                const std::string& client_id) {
    if (!validate_stop_limit_order(symbol, side, stop_price, limit_price, quantity, client_id)) {
        return 0;
    }
    
    uint64_t order_id = next_order_id++;
    
    std::lock_guard<std::mutex> lock(engine_mutex);
    
    if (order_books.find(symbol) == order_books.end()) {
        order_books[symbol] = std::make_shared<OrderBook>(symbol);
        order_books[symbol]->set_trade_callback([this](const std::string& sym, double price, double volume) {
            feed_trade_to_vwap_calculator(sym, price, volume);
        });
    }
    
    auto order = std::make_shared<Order>(order_id, symbol, OrderType::STOP_LIMIT, side, 
                                        stop_price, limit_price, quantity, client_id, StopLimitOrderTag{});
    
    order_books[symbol]->add_order(order);
    thread_pool.enqueue([this, symbol]() {
        process_matching(symbol);
    });
    
    client_orders[client_id].push_back(order_id);
    return order_id;
}

uint64_t MatchingEngine::submit_trailing_stop_order(const std::string& symbol, OrderSide side,
                                                   double trailing_amount, double quantity, 
                                                   const std::string& client_id) {
    if (!validate_trailing_stop_order(symbol, side, trailing_amount, quantity, client_id)) {
        return 0;
    }
    
    uint64_t order_id = next_order_id++;
    
    std::lock_guard<std::mutex> lock(engine_mutex);
    
    if (order_books.find(symbol) == order_books.end()) {
        order_books[symbol] = std::make_shared<OrderBook>(symbol);
        order_books[symbol]->set_trade_callback([this](const std::string& sym, double price, double volume) {
            feed_trade_to_vwap_calculator(sym, price, volume);
        });
    }
    
    auto order = std::make_shared<Order>(order_id, symbol, OrderType::TRAILING_STOP, side, 
                                        trailing_amount, quantity, client_id, TrailingStopOrderTag{});
    
    order_books[symbol]->add_order(order);
    thread_pool.enqueue([this, symbol]() {
        process_matching(symbol);
    });
    
    client_orders[client_id].push_back(order_id);
    return order_id;
}

uint64_t MatchingEngine::submit_vwap_order(const std::string& symbol, OrderSide side,
                                          double target_vwap, double quantity,
                                          std::chrono::steady_clock::time_point start_time,
                                          std::chrono::steady_clock::time_point end_time,
                                          const std::string& client_id) {
    
    if (!validate_vwap_order(symbol, side, target_vwap, quantity, start_time, end_time, client_id)) {
        return 0;
    }
    
    uint64_t order_id = next_order_id++;
    
    std::lock_guard<std::mutex> lock(engine_mutex);
    
    if (vwap_calculators.find(symbol) == vwap_calculators.end()) {
        vwap_calculators[symbol] = std::make_shared<VWAPCalculator>(start_time, end_time);
    }
    
    auto order = std::make_shared<Order>(order_id, symbol, OrderType::VWAP, side, 
                                        target_vwap, quantity, start_time, end_time, 
                                        client_id, VWAPOrderTag{});
    
    client_orders[client_id].push_back(order_id);
    vwap_orders[order_id] = order;
    
    thread_pool.enqueue([this, symbol, order_id]() {
        process_vwap_order(symbol, order_id);
    });
    
    return order_id;
}

bool MatchingEngine::cancel_order(uint64_t order_id, const std::string& client_id) {
    std::lock_guard<std::mutex> lock(engine_mutex);
    
    auto& orders = client_orders[client_id];
    auto it = std::find(orders.begin(), orders.end(), order_id);
    if (it == orders.end()) {
        return false;
    }
    
    auto vwap_it = vwap_orders.find(order_id);
    if (vwap_it != vwap_orders.end()) {
        auto vwap_order = vwap_it->second;
        for (uint64_t child_id : vwap_order->child_order_ids) {
            for (auto& [symbol, book] : order_books) {
                book->cancel_order(child_id);
            }
        }
        
        vwap_order->status = OrderStatus::CANCELLED;
        vwap_orders.erase(vwap_it);
        
        std::cout << "VWAP order " << order_id << " cancelled with " 
                  << vwap_order->child_order_ids.size() << " child orders" << std::endl;
    } else {
        for (auto& [symbol, book] : order_books) {
            book->cancel_order(order_id);
        }
    }
    
    orders.erase(it);
    return true;
}

std::shared_ptr<OrderBook> MatchingEngine::get_order_book(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(engine_mutex);
    auto it = order_books.find(symbol);
    return (it != order_books.end()) ? it->second : nullptr;
}

std::shared_ptr<Order> MatchingEngine::get_vwap_order(uint64_t order_id) {
    std::lock_guard<std::mutex> lock(engine_mutex);
    auto it = vwap_orders.find(order_id);
    return (it != vwap_orders.end()) ? it->second : nullptr;
}

std::vector<std::shared_ptr<Order>> MatchingEngine::get_active_vwap_orders() {
    std::lock_guard<std::mutex> lock(engine_mutex);
    std::vector<std::shared_ptr<Order>> active_orders;
    for (const auto& [id, order] : vwap_orders) {
        active_orders.push_back(order);
    }
    return active_orders;
}

void MatchingEngine::process_matching(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(engine_mutex);
    
    auto book = order_books[symbol];
    if (!book) return;
        
    auto matched_orders = book->match_orders();
    
    if (!matched_orders.empty()) {
        book->check_stop_loss_orders();
        
        update_vwap_order_progress(matched_orders);
    }
    
    for (const auto& order : matched_orders) {
        std::cout << "Order " << order->id << " status: " 
                  << (order->status == OrderStatus::FILLED ? "FILLED" : "PARTIAL") << std::endl;
    }
}

bool MatchingEngine::validate_order(const std::string& symbol, OrderType type, OrderSide side,
                                   double price, double quantity, const std::string& client_id) {
    (void)side;
    if (symbol.empty() || client_id.empty()) return false;
    if (quantity <= 0) return false;
    if (type == OrderType::LIMIT && price <= 0) return false;
    
    return true;
}

bool MatchingEngine::validate_stop_limit_order(const std::string& symbol, OrderSide side,
                                              double stop_price, double limit_price, double quantity, 
                                              const std::string& client_id) {
    if (symbol.empty() || client_id.empty()) return false;
    if (quantity <= 0) return false;
    if (stop_price <= 0 || limit_price <= 0) return false;
    if (side == OrderSide::SELL && stop_price < limit_price) return false;
    if (side == OrderSide::BUY && stop_price > limit_price) return false;
    
    return true;
}

bool MatchingEngine::validate_trailing_stop_order(const std::string& symbol, OrderSide side,
                                                 double trailing_amount, double quantity, 
                                                 const std::string& client_id) {
    if (symbol.empty() || client_id.empty()) return false;
    if (quantity <= 0) return false;
    if (trailing_amount <= 0) return false;
    
    return true;
}

bool MatchingEngine::validate_vwap_order(const std::string& symbol, OrderSide side,
                                        double target_vwap, double quantity,
                                        std::chrono::steady_clock::time_point start_time,
                                        std::chrono::steady_clock::time_point end_time,
                                        const std::string& client_id) {
    if (symbol.empty() || client_id.empty()) return false;
    if (quantity <= 0) return false;
    if (target_vwap <= 0) return false;
    if (start_time >= end_time) return false;
    if (end_time <= std::chrono::steady_clock::now()) return false;
    
    return true;
}

void MatchingEngine::execute_market_buy_order(std::shared_ptr<OrderBook> book, std::shared_ptr<Order> buy_order) {
    double executed_quantity = book->execute_market_order(buy_order, OrderSide::SELL, buy_order->quantity);
    
    if (executed_quantity == buy_order->quantity) {
        buy_order->status = OrderStatus::FILLED;
        std::cout << "Market BUY order " << buy_order->id << " fully filled: " << executed_quantity << " shares" << std::endl;
    } else if (executed_quantity > 0) {
        buy_order->status = OrderStatus::PARTIAL_FILLED;
        std::cout << "Market BUY order " << buy_order->id << " partially filled: " << executed_quantity << "/" << buy_order->quantity << " shares" << std::endl;
    } else {
        buy_order->status = OrderStatus::REJECTED;
        std::cout << "Market BUY order " << buy_order->id << " rejected: no liquidity" << std::endl;
    }
    book->check_stop_loss_orders();
}

void MatchingEngine::execute_market_sell_order(std::shared_ptr<OrderBook> book, std::shared_ptr<Order> sell_order) {
    double executed_quantity = book->execute_market_order(sell_order, OrderSide::BUY, sell_order->quantity);
    
    if (executed_quantity == sell_order->quantity) {
        sell_order->status = OrderStatus::FILLED;
        std::cout << "Market SELL order " << sell_order->id << " fully filled: " << executed_quantity << " shares" << std::endl;
    } else if (executed_quantity > 0) {
        sell_order->status = OrderStatus::PARTIAL_FILLED;
        std::cout << "Market SELL order " << sell_order->id << " partially filled: " << executed_quantity << "/" << sell_order->quantity << " shares" << std::endl;
    } else {
        sell_order->status = OrderStatus::REJECTED;
        std::cout << "Market SELL order " << sell_order->id << " rejected: no liquidity" << std::endl;
    }
    book->check_stop_loss_orders();
}

void MatchingEngine::process_vwap_order(const std::string& symbol, uint64_t order_id) {
    std::lock_guard<std::mutex> lock(engine_mutex);
    
    auto vwap_order_it = vwap_orders.find(order_id);
    if (vwap_order_it == vwap_orders.end()) {
        return;
    }
    
    auto vwap_order = vwap_order_it->second;
    auto calculator_it = vwap_calculators.find(symbol);
    if (calculator_it == vwap_calculators.end()) {
        return;
    }
    
    auto calculator = calculator_it->second;
    auto book = order_books[symbol];
    if (!book) {
        return;
    }
    
    double remaining_quantity = vwap_order->quantity - vwap_order->filled_quantity;
    if (remaining_quantity <= 0) {
        vwap_order->status = OrderStatus::FILLED;
        vwap_orders.erase(vwap_order_it);
        return;
    }
    
    auto params = calculator->calculate_child_order_params(vwap_order, remaining_quantity, vwap_order->target_vwap);
    
    if (params.should_place && params.quantity > 0) {
        uint64_t child_order_id = next_order_id++;
        auto child_order = std::make_shared<Order>(child_order_id, symbol, OrderType::LIMIT, 
                                                  vwap_order->side, params.limit_price, 
                                                  params.quantity, vwap_order->client_id);
        
        book->add_order(child_order);
        
        vwap_order->child_order_ids.push_back(child_order_id);
        vwap_order->last_child_order_price = params.limit_price;
        vwap_order->last_child_order_time = std::chrono::steady_clock::now();
        
        thread_pool.enqueue([this, symbol]() {
            process_matching(symbol);
        });
    }
    
    thread_pool.enqueue([this, symbol, order_id]() {
        std::this_thread::sleep_for(std::chrono::seconds(30));
        process_vwap_order(symbol, order_id);
    });
}

void MatchingEngine::feed_trade_to_vwap_calculator(const std::string& symbol, double price, double volume) {
    auto calculator_it = vwap_calculators.find(symbol);
    if (calculator_it != vwap_calculators.end()) {
        calculator_it->second->add_trade(price, volume);
    }
}

void MatchingEngine::update_vwap_order_progress(const std::vector<std::shared_ptr<Order>>& matched_orders) {
    for (const auto& matched_order : matched_orders) {
        for (auto& [vwap_order_id, vwap_order] : vwap_orders) {
            auto child_it = std::find(vwap_order->child_order_ids.begin(), 
                                     vwap_order->child_order_ids.end(), 
                                     matched_order->id);
            
            if (child_it != vwap_order->child_order_ids.end()) {
                double previous_filled = vwap_order->filled_quantity;
                double child_filled = matched_order->filled_quantity;
                
                double child_contribution = std::min(child_filled, matched_order->quantity);
                
                vwap_order->filled_quantity = previous_filled + child_contribution;
                
                std::cout << "VWAP order " << vwap_order_id << " progress: " 
                          << vwap_order->filled_quantity << "/" << vwap_order->quantity 
                          << " (child order " << matched_order->id << " contributed " 
                          << child_contribution << ")" << std::endl;
                
                if (vwap_order->filled_quantity >= vwap_order->quantity) {
                    vwap_order->status = OrderStatus::FILLED;
                    std::cout << "VWAP order " << vwap_order_id << " completed!" << std::endl;
                    
                    vwap_orders.erase(vwap_order_id);
                }
                
                break;
            }
        }
    }
}
