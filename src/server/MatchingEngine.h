#pragma once
#include "../common/Order.h"
#include "../common/OrderBook.h"
#include "../common/ThreadPool.h"
#include "../common/VWAPCalculator.h"
#include <unordered_map>
#include <memory>
#include <atomic>
#include <mutex>

class MatchingEngine {
private:
    std::unordered_map<std::string, std::shared_ptr<OrderBook>> order_books;
    std::unordered_map<std::string, std::vector<uint64_t>> client_orders;
    std::unordered_map<std::string, std::shared_ptr<VWAPCalculator>> vwap_calculators;
    std::unordered_map<uint64_t, std::shared_ptr<Order>> vwap_orders;
    std::atomic<uint64_t> next_order_id;
    std::mutex engine_mutex;
    ThreadPool thread_pool;
    
public:
    MatchingEngine();
    
    uint64_t submit_order(const std::string& symbol, OrderType type, OrderSide side,
                         double price, double quantity, const std::string& client_id);
    
    uint64_t submit_stop_limit_order(const std::string& symbol, OrderSide side,
                                    double stop_price, double limit_price, double quantity, 
                                    const std::string& client_id);
    
    uint64_t submit_trailing_stop_order(const std::string& symbol, OrderSide side,
                                       double trailing_amount, double quantity, 
                                       const std::string& client_id);
    
    uint64_t submit_vwap_order(const std::string& symbol, OrderSide side,
                              double target_vwap, double quantity,
                              std::chrono::steady_clock::time_point start_time,
                              std::chrono::steady_clock::time_point end_time,
                              const std::string& client_id);
    
    bool cancel_order(uint64_t order_id, const std::string& client_id);
    
    std::shared_ptr<OrderBook> get_order_book(const std::string& symbol);
    
    std::shared_ptr<Order> get_vwap_order(uint64_t order_id);
    std::vector<std::shared_ptr<Order>> get_active_vwap_orders();
    
private:
    void process_matching(const std::string& symbol);
    bool validate_order(const std::string& symbol, OrderType type, OrderSide side,
                       double price, double quantity, const std::string& client_id);
    bool validate_stop_limit_order(const std::string& symbol, OrderSide side,
                                  double stop_price, double limit_price, double quantity, 
                                  const std::string& client_id);
    bool validate_trailing_stop_order(const std::string& symbol, OrderSide side,
                                     double trailing_amount, double quantity, 
                                     const std::string& client_id);
    bool validate_vwap_order(const std::string& symbol, OrderSide side,
                            double target_vwap, double quantity,
                            std::chrono::steady_clock::time_point start_time,
                            std::chrono::steady_clock::time_point end_time,
                            const std::string& client_id);
    void execute_market_buy_order(std::shared_ptr<OrderBook> book, std::shared_ptr<Order> buy_order);
    void execute_market_sell_order(std::shared_ptr<OrderBook> book, std::shared_ptr<Order> sell_order);
    void process_vwap_order(const std::string& symbol, uint64_t order_id);
    void feed_trade_to_vwap_calculator(const std::string& symbol, double price, double volume);
    void update_vwap_order_progress(const std::vector<std::shared_ptr<Order>>& matched_orders);
};