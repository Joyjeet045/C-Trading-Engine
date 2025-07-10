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
    
    if (type == OrderType::MARKET) {
        auto book = order_books[symbol];
        if (side == OrderSide::BUY) {
            order->price = book->get_best_ask();
        } else {
            order->price = book->get_best_bid();
        }
    }
    
    order_books[symbol]->add_order(order);
    
    thread_pool.enqueue([this, symbol]() {
        process_matching(symbol);
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
    if (symbol.empty() || client_id.empty()) return false;
    if (quantity <= 0) return false;
    if (type == OrderType::LIMIT && price <= 0) return false;
    
    return true;
}
