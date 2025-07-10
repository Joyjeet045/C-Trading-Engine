#include "OrderBook.h"
#include <algorithm>
#include <iostream>

OrderBook::OrderBook(const std::string& _symbol) : symbol(_symbol), last_trade_price(0.0) {}

void OrderBook::add_order(std::shared_ptr<Order> order) {
    std::lock_guard<std::mutex> lock(book_mutex);
    
    if (order->type == OrderType::STOP_LOSS) {
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
    
    for (auto it = stop_loss_orders.begin(); it != stop_loss_orders.end();) {
        auto order = *it;
        bool trigger = false;
        
        if (order->side == OrderSide::SELL && last_trade_price <= order->price) {
            trigger = true;
        } else if (order->side == OrderSide::BUY && last_trade_price >= order->price) {
            trigger = true;
        }
        
        if (trigger) {
            order->type = OrderType::MARKET;
            if (order->side == OrderSide::BUY) {
                buy_orders[order->price].push_back(order);
            } else {
                sell_orders[order->price].push_back(order);
            }
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

bool OrderBook::execute_trade(std::shared_ptr<Order> buy_order, std::shared_ptr<Order> sell_order) {
    double trade_quantity = std::min(buy_order->quantity - buy_order->filled_quantity,
                                   sell_order->quantity - sell_order->filled_quantity);
    
    if (trade_quantity <= 0) return false;
    
    double trade_price = sell_order->price; 
    
    buy_order->filled_quantity += trade_quantity;
    sell_order->filled_quantity += trade_quantity;
    
    if (buy_order->filled_quantity >= buy_order->quantity) {
        buy_order->status = OrderStatus::FILLED;
    }
    if (sell_order->filled_quantity >= sell_order->quantity) {
        sell_order->status = OrderStatus::FILLED;
    }
    
    last_trade_price = trade_price;
    
    std::cout << "Trade executed: " << trade_quantity << " @ " << trade_price << std::endl;
    return true;
}
