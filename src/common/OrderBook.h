#pragma once
#include "Order.h"
#include <map>
#include <vector>
#include <mutex>
#include <memory>

class OrderBook {
private:
    std::map<double, std::vector<std::shared_ptr<Order>>> buy_orders;
    std::map<double, std::vector<std::shared_ptr<Order>>> sell_orders;
    std::vector<std::shared_ptr<Order>> stop_loss_orders;
    mutable std::mutex book_mutex;
    double last_trade_price;
    std::string symbol;

public:
    OrderBook(const std::string& _symbol);
    
    void add_order(std::shared_ptr<Order> order);
    void cancel_order(uint64_t order_id);
    std::vector<std::shared_ptr<Order>> match_orders();
    void check_stop_loss_orders();
    double execute_market_order(std::shared_ptr<Order> market_order, OrderSide opposite_side, double max_quantity);
    
    double get_best_bid() const;
    double get_best_ask() const;
    double get_last_price() const { return last_trade_price; }
    
private:
    void remove_order_from_book(std::shared_ptr<Order> order);
    bool execute_trade(std::shared_ptr<Order> buy_order, std::shared_ptr<Order> sell_order);
};