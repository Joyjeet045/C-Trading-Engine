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
    
    if (price <= 0.0 || volume <= 0.0) {
        return;
    }
    
    vwap_accumulator += price * volume;
    volume_accumulator += volume;
    current_vwap = (volume_accumulator > 0) ? vwap_accumulator / volume_accumulator : 0.0;
    
    if (now >= start_time && now <= end_time) {
        Trade trade{price, volume, now};
        rolling_trades.push_back(trade);
        rolling_vwap_accumulator += price * volume;
        rolling_volume_accumulator += volume;
        
        update_rolling_window();
    }
}

double VWAPCalculator::get_rolling_vwap() const {
    return (rolling_volume_accumulator > 0) ? rolling_vwap_accumulator / rolling_volume_accumulator : 0.0;
}

void VWAPCalculator::update_rolling_window() {
    auto now = std::chrono::steady_clock::now();
    auto five_minutes_ago = now - std::chrono::minutes(5);
    
    auto it = rolling_trades.begin();
    while (it != rolling_trades.end() && it->timestamp < five_minutes_ago) {
        const auto& old_trade = *it;
        rolling_vwap_accumulator -= old_trade.price * old_trade.volume;
        rolling_volume_accumulator -= old_trade.volume;
        ++it;
    }
    
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
    
    if (!vwap_order || remaining_quantity <= 0.0 || target_vwap <= 0.0) {
        params.should_place = false;
        return params;
    }
    
    if (now < vwap_order->execution_start_time || now > vwap_order->execution_end_time) {
        params.should_place = false;
        return params;
    }
    
    auto time_remaining = std::chrono::duration_cast<std::chrono::seconds>(
        vwap_order->execution_end_time - now).count();
    
    if (time_remaining <= 0) {
        params.should_place = false;
        return params;
    }
    
    params.quantity = calculate_optimal_quantity(remaining_quantity, time_remaining, target_vwap);
    
    double vwap_deviation = calculate_deviation(current_vwap, target_vwap);
    double rolling_vwap = get_rolling_vwap();
    
    if (vwap_order->side == OrderSide::BUY) {
        if (current_vwap <= target_vwap) {
            params.limit_price = target_vwap;
        } else {
            if (vwap_deviation <= 0.01) {
                params.limit_price = target_vwap * 0.999;
            } else {
                params.should_place = false;
                return params;
            }
        }
    } else {
        if (current_vwap >= target_vwap) {
            params.limit_price = target_vwap;
        } else {
            if (vwap_deviation >= -0.01) {
                params.limit_price = target_vwap * 1.001;
            } else {
                params.should_place = false;
                return params;
            }
        }
    }
    
    auto time_since_last = std::chrono::duration_cast<std::chrono::seconds>(
        now - vwap_order->last_child_order_time).count();
    
    double price_change = std::abs(params.limit_price - vwap_order->last_child_order_price);
    double price_change_pct = price_change / target_vwap;
    
    params.should_place = (time_since_last >= 30) || (price_change_pct >= 0.001);
    
    return params;
}

double VWAPCalculator::calculate_deviation(double current_price, double target_price) {
    return (current_price - target_price) / target_price;
}

double VWAPCalculator::calculate_optimal_quantity(double remaining_quantity, double time_remaining, double target_vwap) {
    double base_quantity = remaining_quantity / (time_remaining / 60.0);
    
    double volume_factor = std::min(2.0, std::max(0.5, rolling_volume_accumulator / 1000.0));
    
    double deviation_factor = 1.0;
    if (std::abs(current_vwap - target_vwap) / target_vwap > 0.01) {
        deviation_factor = 1.5;
    }
    
    return std::min(remaining_quantity, base_quantity * volume_factor * deviation_factor);
} 