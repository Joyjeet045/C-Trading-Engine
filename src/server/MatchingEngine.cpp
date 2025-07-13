#include "MatchingEngine.h"
#include <iostream>
#include <algorithm>

MatchingEngine::MatchingEngine() : next_order_id(1), thread_pool(4) {}

uint64_t MatchingEngine::submit_order(const std::string& symbol, OrderType type, OrderSide side,
                                     double price, double quantity, const std::string& client_id) {
    
    if (!validate_order(symbol, type, side, price, quantity, client_id)) {
        return 0; 
    }
    
    uint64_t order_id = next_order_id++;
    
    std::lock_guard<std::mutex> lock(engine_mutex);
    
    if (order_books.find(symbol) == order_books.end()) {
        order_books[symbol] = std::make_shared<OrderBook>(symbol);
    }
    
    auto order = std::make_shared<Order>(order_id, symbol, type, side, price, quantity, client_id);
    client_orders[client_id].push_back(order_id);
    
    auto book = order_books[symbol];
    
    if (type == OrderType::MARKET) {
        if (side == OrderSide::BUY) {
            execute_market_buy_order(book, order);
        } else {
            execute_market_sell_order(book, order);
        }
    } else {
        book->add_order(order);
        thread_pool.enqueue([this, symbol]() {
            process_matching(symbol);
        });
    }
    
    return order_id;
}

bool MatchingEngine::cancel_order(uint64_t order_id, const std::string& client_id) {
    std::lock_guard<std::mutex> lock(engine_mutex);
    
    auto& orders = client_orders[client_id];
    auto it = std::find(orders.begin(), orders.end(), order_id);
    if (it == orders.end()) {
        return false; 
    }
    
    for (auto& [symbol, book] : order_books) {
        book->cancel_order(order_id);
    }
    
    orders.erase(it);
    return true;
}

std::shared_ptr<OrderBook> MatchingEngine::get_order_book(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(engine_mutex);
    auto it = order_books.find(symbol);
    return (it != order_books.end()) ? it->second : nullptr;
}

void MatchingEngine::process_matching(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(engine_mutex);
    
    auto book = order_books[symbol];
    if (!book) return;
    
    book->check_stop_loss_orders();
    
    auto matched_orders = book->match_orders();
    
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
