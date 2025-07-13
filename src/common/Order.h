#pragma once
#include <string>
#include <chrono>

enum class OrderType {
    MARKET,
    LIMIT,
    STOP_LOSS
};

enum class OrderSide {
    BUY,
    SELL
};

enum class OrderStatus {
    PENDING,
    FILLED,
    PARTIAL_FILLED,
    CANCELLED,
    REJECTED
};

struct Order {
    uint64_t id;
    std::string symbol;
    OrderType type;
    OrderSide side;
    double price;
    double quantity;
    double filled_quantity;
    OrderStatus status;
    std::string client_id;
    std::chrono::steady_clock::time_point timestamp;
    
    Order(uint64_t _id, const std::string& _symbol, OrderType _type, 
          OrderSide _side, double _price, double _quantity, const std::string& _client_id)
        : id(_id), symbol(_symbol), type(_type), side(_side), price(_price), 
          quantity(_quantity), filled_quantity(0), status(OrderStatus::PENDING),
          client_id(_client_id), timestamp(std::chrono::steady_clock::now()) {}
};
