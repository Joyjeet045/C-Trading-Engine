#pragma once
#include <string>
#include <chrono>
#include <vector>

enum class OrderType {
    MARKET,
    LIMIT,
    STOP_LOSS,
    STOP_LIMIT,
    TRAILING_STOP,
    VWAP
};

enum class OrderSide {
    BUY,
    SELL
};

enum class OrderStatus {
    PENDING,
    PARTIAL_FILLED,
    FILLED,
    CANCELLED,
    REJECTED
};

struct StopLimitOrderTag {};
struct TrailingStopOrderTag {};
struct VWAPOrderTag {};

class Order {
public:
    uint64_t id;
    std::string symbol;
    OrderType type;
    OrderSide side;
    double price;
    double quantity;
    std::string client_id;
    OrderStatus status;
    double filled_quantity;
    std::chrono::steady_clock::time_point timestamp;
    
    double limit_price;
    double stop_price;
    double trailing_amount;
    double highest_price;
    double lowest_price;
    
    double target_vwap;
    double vwap_accumulator;
    double volume_accumulator;
    std::chrono::steady_clock::time_point execution_start_time;
    std::chrono::steady_clock::time_point execution_end_time;
    std::vector<uint64_t> child_order_ids;
    double last_child_order_price;
    std::chrono::steady_clock::time_point last_child_order_time;
    
    Order(uint64_t _id, const std::string& _symbol, OrderType _type, OrderSide _side, 
          double _price, double _quantity, const std::string& _client_id)
        : id(_id), symbol(_symbol), type(_type), side(_side), price(_price), 
          quantity(_quantity), client_id(_client_id), status(OrderStatus::PENDING),
          filled_quantity(0.0), timestamp(std::chrono::steady_clock::now()),
          limit_price(0.0), stop_price(0.0), trailing_amount(0.0), 
          highest_price(0.0), lowest_price(0.0), target_vwap(0.0),
          vwap_accumulator(0.0), volume_accumulator(0.0), last_child_order_price(0.0),
          last_child_order_time(std::chrono::steady_clock::now()) {}
    
    Order(uint64_t _id, const std::string& _symbol, OrderType _type, OrderSide _side,
          double _stop_price, double _limit_price, double _quantity, 
          const std::string& _client_id, StopLimitOrderTag)
        : id(_id), symbol(_symbol), type(_type), side(_side), price(_stop_price),
          quantity(_quantity), client_id(_client_id), status(OrderStatus::PENDING),
          filled_quantity(0.0), timestamp(std::chrono::steady_clock::now()),
          limit_price(_limit_price), stop_price(_stop_price), trailing_amount(0.0),
          highest_price(0.0), lowest_price(0.0), target_vwap(0.0),
          vwap_accumulator(0.0), volume_accumulator(0.0), last_child_order_price(0.0),
          last_child_order_time(std::chrono::steady_clock::now()) {}
    
    Order(uint64_t _id, const std::string& _symbol, OrderType _type, OrderSide _side,
          double _trailing_amount, double _quantity, const std::string& _client_id, TrailingStopOrderTag)
        : id(_id), symbol(_symbol), type(_type), side(_side), price(0.0),
          quantity(_quantity), client_id(_client_id), status(OrderStatus::PENDING),
          filled_quantity(0.0), timestamp(std::chrono::steady_clock::now()),
          limit_price(0.0), stop_price(0.0), trailing_amount(_trailing_amount),
          highest_price(0.0), lowest_price(0.0), target_vwap(0.0),
          vwap_accumulator(0.0), volume_accumulator(0.0), last_child_order_price(0.0),
          last_child_order_time(std::chrono::steady_clock::now()) {}
    
    Order(uint64_t _id, const std::string& _symbol, OrderType _type, OrderSide _side,
          double _target_vwap, double _quantity, 
          std::chrono::steady_clock::time_point _start_time,
          std::chrono::steady_clock::time_point _end_time,
          const std::string& _client_id, VWAPOrderTag)
        : id(_id), symbol(_symbol), type(_type), side(_side), price(_target_vwap),
          quantity(_quantity), client_id(_client_id), status(OrderStatus::PENDING),
          filled_quantity(0.0), timestamp(std::chrono::steady_clock::now()),
          limit_price(0.0), stop_price(0.0), trailing_amount(0.0),
          highest_price(0.0), lowest_price(0.0), target_vwap(_target_vwap),
          execution_start_time(_start_time), execution_end_time(_end_time),
          vwap_accumulator(0.0), volume_accumulator(0.0), last_child_order_price(0.0),
          last_child_order_time(std::chrono::steady_clock::now()) {}
};
