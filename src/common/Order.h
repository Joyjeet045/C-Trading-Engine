#pragma once
#include <string>
#include <chrono>

enum class OrderType {
    MARKET,
    LIMIT,
    STOP_LOSS,
    STOP_LIMIT,
    TRAILING_STOP
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

// Tag structs for constructor overloading
struct RegularOrderTag {};
struct StopLimitOrderTag {};
struct TrailingStopOrderTag {};

struct Order {
    uint64_t id;
    std::string symbol;
    OrderType type;
    OrderSide side;
    double price;           // Stop price for stop orders, limit price for regular orders
    double limit_price;     // Limit price for stop limit orders (only used when type is STOP_LIMIT)
    double trailing_amount; // Fixed dollar amount for trailing stop orders
    double highest_price;   // Track highest price for SELL trailing stops
    double lowest_price;    // Track lowest price for BUY trailing stops
    double quantity;
    double filled_quantity;
    OrderStatus status;
    std::string client_id;
    std::chrono::steady_clock::time_point timestamp;
    
    // Constructor for regular orders (MARKET, LIMIT, STOP_LOSS)
    Order(uint64_t _id, const std::string& _symbol, OrderType _type, 
          OrderSide _side, double _price, double _quantity, const std::string& _client_id, RegularOrderTag)
        : id(_id), symbol(_symbol), type(_type), side(_side), price(_price), 
          limit_price(_price), trailing_amount(0.0), highest_price(_price), lowest_price(_price),
          quantity(_quantity), filled_quantity(0), status(OrderStatus::PENDING),
          client_id(_client_id), timestamp(std::chrono::steady_clock::now()) {}
    
    // Constructor for stop limit orders with separate stop and limit prices
    Order(uint64_t _id, const std::string& _symbol, OrderType _type, 
          OrderSide _side, double _stop_price, double _limit_price, double _quantity, const std::string& _client_id, StopLimitOrderTag)
        : id(_id), symbol(_symbol), type(_type), side(_side), price(_stop_price), 
          limit_price(_limit_price), trailing_amount(0.0), highest_price(_stop_price), lowest_price(_stop_price),
          quantity(_quantity), filled_quantity(0), status(OrderStatus::PENDING),
          client_id(_client_id), timestamp(std::chrono::steady_clock::now()) {}
    
    // Constructor for trailing stop orders with trailing amount
    Order(uint64_t _id, const std::string& _symbol, OrderType _type, 
          OrderSide _side, double _trailing_amount, double _quantity, const std::string& _client_id, TrailingStopOrderTag)
        : id(_id), symbol(_symbol), type(_type), side(_side), price(0.0), 
          limit_price(0.0), trailing_amount(_trailing_amount), highest_price(0.0), lowest_price(0.0),
          quantity(_quantity), filled_quantity(0), status(OrderStatus::PENDING),
          client_id(_client_id), timestamp(std::chrono::steady_clock::now()) {}
    
    // Convenience constructors that automatically select the right tag
    Order(uint64_t _id, const std::string& _symbol, OrderType _type, 
          OrderSide _side, double _price, double _quantity, const std::string& _client_id)
        : Order(_id, _symbol, _type, _side, _price, _quantity, _client_id, RegularOrderTag{}) {}
};
