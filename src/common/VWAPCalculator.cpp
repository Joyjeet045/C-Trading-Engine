#include "VWAPCalculator.h"
#include <algorithm>
#include <cmath>

VWAPCalculator::VWAPCalculator(std::chrono::steady_clock::time_point start, 
                               std::chrono::steady_clock::time_point end)
    : vwap_accumulator(0.0), volume_accumulator(0.0), current_vwap(0.0),
      start_time(start), end_time(end),
      rolling_vwap_accumulator(0.0), rolling_volume_accumulator(0.0) {}

void VWAPCalculator::add_trade(double price, double volume) {
    auto now = std::chrono::steady_clock::now();
    
    // Validate inputs
    if (price <= 0.0 || volume <= 0.0) {
        return; // Invalid trade data
    }
    
    // Process all trades to build accurate VWAP, but only use them for calculations
    // within the execution window
    vwap_accumulator += price * volume;
    volume_accumulator += volume;
    current_vwap = (volume_accumulator > 0) ? vwap_accumulator / volume_accumulator : 0.0;
    
    // Add to rolling window (only if within execution window)
    if (now >= start_time && now <= end_time) {
        Trade trade{price, volume, now};
        rolling_trades.push_back(trade);
        rolling_vwap_accumulator += price * volume;
        rolling_volume_accumulator += volume;
        
        // Update rolling window (keep last 5 minutes)
        update_rolling_window();
    }
}

double VWAPCalculator::get_rolling_vwap() const {
    return (rolling_volume_accumulator > 0) ? rolling_vwap_accumulator / rolling_volume_accumulator : 0.0;
}

void VWAPCalculator::update_rolling_window() {
    auto now = std::chrono::steady_clock::now();
    auto five_minutes_ago = now - std::chrono::minutes(5);
    
    // Find the first trade that's within the 5-minute window
    auto it = rolling_trades.begin();
    while (it != rolling_trades.end() && it->timestamp < five_minutes_ago) {
        const auto& old_trade = *it;
        rolling_vwap_accumulator -= old_trade.price * old_trade.volume;
        rolling_volume_accumulator -= old_trade.volume;
        ++it;
    }
    
    // Remove all old trades at once
    if (it != rolling_trades.begin()) {
        rolling_trades.erase(rolling_trades.begin(), it);
    }
}

VWAPCalculator::ChildOrderParams VWAPCalculator::calculate_child_order_params(
    std::shared_ptr<Order> vwap_order,
    double remaining_quantity,
    double target_vwap) {
    
    ChildOrderParams params;
    auto now = std::chrono::steady_clock::now();
    
    // Validate inputs
    if (!vwap_order || remaining_quantity <= 0.0 || target_vwap <= 0.0) {
        params.should_place = false;
        return params;
    }
    
    // Check if we're in execution window
    if (now < vwap_order->execution_start_time || now > vwap_order->execution_end_time) {
        params.should_place = false;
        return params;
    }
    
    // Calculate time remaining
    auto time_remaining = std::chrono::duration_cast<std::chrono::seconds>(
        vwap_order->execution_end_time - now).count();
    
    if (time_remaining <= 0) {
        params.should_place = false;
        return params;
    }
    
    // Calculate optimal quantity based on time remaining
    params.quantity = calculate_optimal_quantity(remaining_quantity, time_remaining, target_vwap);
    
    // Calculate limit price based on VWAP deviation
    double vwap_deviation = calculate_deviation(current_vwap, target_vwap);
    double rolling_vwap = get_rolling_vwap();
    
    if (vwap_order->side == OrderSide::BUY) {
        if (current_vwap <= target_vwap) {
            // VWAP is at or below target - we can buy at target or better
            params.limit_price = target_vwap;
        } else {
            // VWAP is above target - be more selective
            // Only place orders if VWAP is close to target (within 1%)
            if (vwap_deviation <= 0.01) {
                // Set limit price slightly below target to get better execution
                params.limit_price = target_vwap * 0.999; // 0.1% below target
            } else {
                // VWAP too high - don't place orders, wait for better prices
                params.should_place = false;
                return params;
            }
        }
    } else {
        // SELL orders
        if (current_vwap >= target_vwap) {
            // VWAP is at or above target - we can sell at target or better
            params.limit_price = target_vwap;
        } else {
            // VWAP is below target - be more selective
            if (vwap_deviation >= -0.01) {
                // Set limit price slightly above target to get better execution
                params.limit_price = target_vwap * 1.001; // 0.1% above target
            } else {
                // VWAP too low - don't place orders, wait for better prices
                params.should_place = false;
                return params;
            }
        }
    }
    
    // Check if we should place a new child order
    // Place if: enough time has passed OR VWAP has moved significantly
    auto time_since_last = std::chrono::duration_cast<std::chrono::seconds>(
        now - vwap_order->last_child_order_time).count();
    
    double price_change = std::abs(params.limit_price - vwap_order->last_child_order_price);
    double price_change_pct = price_change / target_vwap;
    
    params.should_place = (time_since_last >= 30) || // At least 30 seconds
                         (price_change_pct >= 0.001); // Or 0.1% price change
    
    return params;
}

double VWAPCalculator::calculate_deviation(double current_price, double target_price) {
    return (current_price - target_price) / target_price;
}

double VWAPCalculator::calculate_optimal_quantity(double remaining_quantity, double time_remaining, double target_vwap) {
    // Base quantity: remaining quantity / remaining time periods
    double base_quantity = remaining_quantity / (time_remaining / 60.0); // Per minute
    
    // Adjust based on volume in rolling window
    double volume_factor = std::min(2.0, std::max(0.5, rolling_volume_accumulator / 1000.0));
    
    // Adjust based on VWAP deviation
    double deviation_factor = 1.0;
    if (std::abs(current_vwap - target_vwap) / target_vwap > 0.01) {
        deviation_factor = 1.5; // Be more aggressive if VWAP is off target
    }
    
    return std::min(remaining_quantity, base_quantity * volume_factor * deviation_factor);
} 