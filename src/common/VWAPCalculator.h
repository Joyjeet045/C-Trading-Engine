#pragma once
#include <vector>
#include <chrono>
#include <memory>
#include "Order.h"

struct Trade {
    double price;
    double volume;
    std::chrono::steady_clock::time_point timestamp;
};

class VWAPCalculator {
private:
    std::vector<Trade> trades;
    double vwap_accumulator;
    double volume_accumulator;
    double current_vwap;
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point end_time;
    
    // Rolling window for VWAP (e.g., last 5 minutes)
    std::vector<Trade> rolling_trades;
    double rolling_vwap_accumulator;
    double rolling_volume_accumulator;
    
public:
    VWAPCalculator(std::chrono::steady_clock::time_point start, 
                   std::chrono::steady_clock::time_point end);
    
    // Called with every trade in the market
    void add_trade(double price, double volume);
    
    // Get current VWAP (full period)
    double get_current_vwap() const { return current_vwap; }
    
    // Get rolling VWAP (recent period)
    double get_rolling_vwap() const;
    
    // Calculate optimal child order parameters
    struct ChildOrderParams {
        double limit_price;
        double quantity;
        bool should_place;
    };
    
    ChildOrderParams calculate_child_order_params(
        std::shared_ptr<Order> vwap_order,
        double remaining_quantity,
        double target_vwap
    );
    
private:
    void update_rolling_window();
    double calculate_deviation(double current_price, double target_price);
    double calculate_optimal_quantity(double remaining_quantity, double time_remaining, double target_vwap);
}; 