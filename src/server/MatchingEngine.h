#pragma once
#include "../common/Order.h"
#include "../common/OrderBook.h"
#include "../common/ThreadPool.h"
#include <unordered_map>
#include <memory>
#include <atomic>
#include <mutex>

class MatchingEngine {
private:
    std::unordered_map<std::string, std::shared_ptr<OrderBook>> order_books;
    std::unordered_map<std::string, std::vector<uint64_t>> client_orders; 
    std::atomic<uint64_t> next_order_id;
    std::mutex engine_mutex;
    ThreadPool thread_pool;
    
public:
    MatchingEngine();
    
    uint64_t submit_order(const std::string& symbol, OrderType type, OrderSide side,
                         double price, double quantity, const std::string& client_id);
    
    bool cancel_order(uint64_t order_id, const std::string& client_id);
    
    std::shared_ptr<OrderBook> get_order_book(const std::string& symbol);
    
private:
    void process_matching(const std::string& symbol);
    bool validate_order(const std::string& symbol, OrderType type, OrderSide side,
                       double price, double quantity, const std::string& client_id);
    void execute_market_buy_order(std::shared_ptr<OrderBook> book, std::shared_ptr<Order> buy_order);
    void execute_market_sell_order(std::shared_ptr<OrderBook> book, std::shared_ptr<Order> sell_order);
};