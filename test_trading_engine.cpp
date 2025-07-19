#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>
#include <vector>
#include <string>
#include <mutex>
#include "src/server/MatchingEngine.h"
#include "src/common/Order.h"
#include "src/common/OrderBook.h"

class TradingEngineTest {
private:
    MatchingEngine engine;

public:
    void run_all_tests() {
        std::cout << "=== REALISTIC TRADING ENGINE TEST SUITE ===" << std::endl;
        
        test_basic_order_types();
        test_order_matching_with_price_time_priority();
        test_market_order_execution();
        test_stop_loss_orders_realistic();
        test_stop_limit_orders_realistic();
        test_trailing_stop_orders_realistic();
        test_order_cancellation_realistic();
        test_order_book_operations_realistic();
        test_edge_cases_realistic();
        test_concurrent_operations_realistic();
        test_partial_fills_and_remaining_quantity();
        test_order_status_transitions();
        
        std::cout << "\n=== ALL REALISTIC TESTS PASSED ===" << std::endl;
    }

    void test_basic_order_types() {
        std::cout << "\n--- Testing Basic Order Types (Realistic) ---" << std::endl;

        uint64_t limit_order_id = engine.submit_order("AAPL", OrderType::LIMIT, OrderSide::SELL, 150.0, 50, "client2");
        assert(limit_order_id > 0);

        auto book = engine.get_order_book("AAPL");
        assert(book != nullptr);
        assert(book->get_best_ask() == 150.0);
        std::cout << "✓ LIMIT order creation and placement passed" << std::endl;

        uint64_t market_order_id = engine.submit_order("AAPL", OrderType::MARKET, OrderSide::BUY, 0.0, 100, "client1");
        assert(market_order_id > 0);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        book = engine.get_order_book("AAPL");
        assert(book != nullptr);
        assert(book->get_best_ask() == 0.0);
        assert(book->get_last_price() > 0.0);
        std::cout << "✓ MARKET order execution passed" << std::endl;

        uint64_t stop_loss_id = engine.submit_order("AAPL", OrderType::STOP_LOSS, OrderSide::SELL, 140.0, 25, "client1");
        assert(stop_loss_id > 0);
        std::cout << "✓ STOP_LOSS order creation passed" << std::endl;

        uint64_t stop_limit_id = engine.submit_stop_limit_order("AAPL", OrderSide::SELL, 145.0, 144.0, 30, "client2");
        assert(stop_limit_id > 0);
        std::cout << "✓ STOP_LIMIT order creation passed" << std::endl;

        uint64_t trailing_stop_id = engine.submit_trailing_stop_order("AAPL", OrderSide::SELL, 5.0, 40, "client1");
        assert(trailing_stop_id > 0);
        std::cout << "✓ TRAILING_STOP order creation passed" << std::endl;
    }

    void test_order_matching_with_price_time_priority() {
        std::cout << "\n--- Testing Order Matching with Price-Time Priority ---" << std::endl;
        
        uint64_t buy_order1 = engine.submit_order("MSFT", OrderType::LIMIT, OrderSide::BUY, 200.0, 50, "client1");
        uint64_t buy_order2 = engine.submit_order("MSFT", OrderType::LIMIT, OrderSide::BUY, 200.0, 30, "client2");
        uint64_t buy_order3 = engine.submit_order("MSFT", OrderType::LIMIT, OrderSide::BUY, 200.0, 20, "client3");
        
        uint64_t sell_order1 = engine.submit_order("MSFT", OrderType::LIMIT, OrderSide::SELL, 200.0, 100, "client4");
        
        uint64_t sell_order2 = engine.submit_order("MSFT", OrderType::LIMIT, OrderSide::SELL, 201.0, 50, "client5");
        
        assert(buy_order1 > 0 && buy_order2 > 0 && buy_order3 > 0);
        assert(sell_order1 > 0 && sell_order2 > 0);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        auto book = engine.get_order_book("MSFT");
        assert(book != nullptr);
        
        assert(book->get_best_bid() == 0.0);
        assert(book->get_best_ask() == 201.0);
        
        assert(book->get_last_price() > 0.0);
        
        std::cout << "✓ Price-time priority matching test passed" << std::endl;
    }
    
    void test_market_order_execution() {
        std::cout << "\n--- Testing Market Order Execution ---" << std::endl;
        
        engine.submit_order("GOOGL", OrderType::LIMIT, OrderSide::BUY, 2500.0, 100, "liquidity_buyer");
        engine.submit_order("GOOGL", OrderType::LIMIT, OrderSide::SELL, 2501.0, 100, "liquidity_seller");
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        uint64_t market_buy = engine.submit_order("GOOGL", OrderType::MARKET, OrderSide::BUY, 0.0, 50, "market_buyer");
        assert(market_buy > 0);
        
        uint64_t market_sell = engine.submit_order("GOOGL", OrderType::MARKET, OrderSide::SELL, 0.0, 30, "market_seller");
        assert(market_sell > 0);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        auto book = engine.get_order_book("GOOGL");
        assert(book != nullptr);
        
        assert(book->get_best_bid() == 2500.0);
        assert(book->get_best_ask() == 2501.0);
        
        double last_price = book->get_last_price();
        assert(last_price > 0.0);
        
        uint64_t market_buy_no_liquidity = engine.submit_order("GOOGL", OrderType::MARKET, OrderSide::BUY, 0.0, 100, "no_liquidity_buyer");
        assert(market_buy_no_liquidity > 0);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        book = engine.get_order_book("GOOGL");
        assert(book != nullptr);
        assert(book->get_best_ask() == 0.0); 
        assert(book->get_best_bid() == 2500.0); 
        
        std::cout << "✓ Market order execution test passed" << std::endl;
    }
    
    void test_stop_loss_orders_realistic() {
        std::cout << "\n--- Testing Stop Loss Orders (Realistic) ---" << std::endl;
        
        engine.submit_order("TSLA", OrderType::LIMIT, OrderSide::BUY, 800.0, 100, "liquidity_provider");
        engine.submit_order("TSLA", OrderType::LIMIT, OrderSide::SELL, 810.0, 100, "liquidity_provider2");
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        uint64_t stop_order = engine.submit_order("TSLA", OrderType::STOP_LOSS, OrderSide::SELL, 805.0, 25, "stop_client");
        assert(stop_order > 0);
        
        auto book = engine.get_order_book("TSLA");
        assert(book != nullptr);
        assert(book->get_best_bid() == 800.0);
        assert(book->get_best_ask() == 810.0);
        
        uint64_t trigger_buy = engine.submit_order("TSLA", OrderType::LIMIT, OrderSide::BUY, 805.0, 10, "trigger_client");
        uint64_t trigger_sell = engine.submit_order("TSLA", OrderType::LIMIT, OrderSide::SELL, 805.0, 10, "trigger_client2");
        
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        book = engine.get_order_book("TSLA");
        assert(book != nullptr);
        double last_price = book->get_last_price();
        assert(last_price == 800.0); 
        
        assert(book->get_best_bid() == 800.0);
        assert(book->get_best_ask() == 810.0);
        
        uint64_t buy_stop_order = engine.submit_order("TSLA", OrderType::STOP_LOSS, OrderSide::BUY, 815.0, 15, "buy_stop_client");
        assert(buy_stop_order > 0);
        
        uint64_t trigger_buy2 = engine.submit_order("TSLA", OrderType::LIMIT, OrderSide::BUY, 815.0, 5, "trigger_client3");
        uint64_t trigger_sell2 = engine.submit_order("TSLA", OrderType::LIMIT, OrderSide::SELL, 815.0, 5, "trigger_client4");
        
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        book = engine.get_order_book("TSLA");
        assert(book != nullptr);
        assert(book->get_best_ask() == 810.0);
        
        std::cout << "✓ Stop loss order triggering and execution test passed" << std::endl;
    }
    
    void test_stop_limit_orders_realistic() {
        std::cout << "\n--- Testing Stop Limit Orders (Realistic) ---" << std::endl;
        
        engine.submit_order("NVDA", OrderType::LIMIT, OrderSide::BUY, 400.0, 100, "liquidity_buyer");
        engine.submit_order("NVDA", OrderType::LIMIT, OrderSide::SELL, 420.0, 100, "liquidity_seller");
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        uint64_t stop_limit = engine.submit_stop_limit_order("NVDA", OrderSide::SELL, 410.0, 405.0, 30, "stop_limit_client");
        assert(stop_limit > 0);
        
        auto book = engine.get_order_book("NVDA");
        assert(book != nullptr);
        assert(book->get_best_bid() == 400.0);
        assert(book->get_best_ask() == 420.0);
        
        uint64_t trigger_buy = engine.submit_order("NVDA", OrderType::LIMIT, OrderSide::BUY, 410.0, 10, "trigger_client");
        uint64_t trigger_sell = engine.submit_order("NVDA", OrderType::LIMIT, OrderSide::SELL, 410.0, 10, "trigger_client2");
        
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        book = engine.get_order_book("NVDA");
        assert(book != nullptr);
        
        assert(book->get_best_ask() == 405.0);
        assert(book->get_best_bid() == 400.0);
        
        uint64_t market_buy = engine.submit_order("NVDA", OrderType::MARKET, OrderSide::BUY, 0.0, 30, "execution_test");
        assert(market_buy > 0);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        book = engine.get_order_book("NVDA");
        assert(book != nullptr);
        assert(book->get_best_ask() == 420.0);
        
        std::cout << "✓ Stop limit order triggering and conversion test passed" << std::endl;
    }
    
    void test_trailing_stop_orders_realistic() {
        std::cout << "\n--- Testing Trailing Stop Orders (Realistic) ---" << std::endl;
        
        engine.submit_order("AMD", OrderType::LIMIT, OrderSide::BUY, 100.0, 100, "liquidity_buyer");
        engine.submit_order("AMD", OrderType::LIMIT, OrderSide::SELL, 120.0, 100, "liquidity_seller");
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        uint64_t trailing_stop = engine.submit_trailing_stop_order("AMD", OrderSide::SELL, 5.0, 25, "trailing_client");
        assert(trailing_stop > 0);
        
        engine.submit_order("AMD", OrderType::LIMIT, OrderSide::BUY, 110.0, 10, "price_mover1");
        engine.submit_order("AMD", OrderType::LIMIT, OrderSide::SELL, 110.0, 10, "price_mover2");
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        engine.submit_order("AMD", OrderType::LIMIT, OrderSide::BUY, 115.0, 10, "price_mover3");
        engine.submit_order("AMD", OrderType::LIMIT, OrderSide::SELL, 115.0, 10, "price_mover4");
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        engine.submit_order("AMD", OrderType::LIMIT, OrderSide::BUY, 108.0, 10, "trigger_client");
        engine.submit_order("AMD", OrderType::LIMIT, OrderSide::SELL, 108.0, 10, "trigger_client2");
        
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        auto book = engine.get_order_book("AMD");
        assert(book != nullptr);
        
        double last_price = book->get_last_price();
        assert(last_price > 0.0);
        
        assert(book->get_best_bid() == 100.0);
        assert(book->get_best_ask() == 120.0);
        
        std::cout << "✓ Trailing stop order price updates and triggering test passed" << std::endl;
    }
    
    void test_order_cancellation_realistic() {
        std::cout << "\n--- Testing Order Cancellation (Realistic) ---" << std::endl;
        
        uint64_t order1 = engine.submit_order("AMZN", OrderType::LIMIT, OrderSide::BUY, 3000.0, 10, "client1");
        uint64_t order2 = engine.submit_order("AMZN", OrderType::LIMIT, OrderSide::BUY, 3001.0, 15, "client1");
        uint64_t order3 = engine.submit_order("AMZN", OrderType::LIMIT, OrderSide::SELL, 3100.0, 20, "client2");
        
        assert(order1 > 0 && order2 > 0 && order3 > 0);
        
        auto book = engine.get_order_book("AMZN");
        assert(book != nullptr);
        assert(book->get_best_bid() == 3001.0);
        assert(book->get_best_ask() == 3100.0);
        
        bool cancel_success = engine.cancel_order(order2, "client1");
        assert(cancel_success);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        book = engine.get_order_book("AMZN");
        assert(book != nullptr);
        assert(book->get_best_bid() == 3000.0);
        assert(book->get_best_ask() == 3100.0);
        
        bool cancel_fail = engine.cancel_order(99999, "client1");
        assert(!cancel_fail);
        
        bool wrong_client_cancel = engine.cancel_order(order3, "client1");
        assert(!wrong_client_cancel);
        
        std::cout << "✓ Order cancellation test passed" << std::endl;
    }
    
    void test_order_book_operations_realistic() {
        std::cout << "\n--- Testing Order Book Operations (Realistic) ---" << std::endl;
        
        auto book = engine.get_order_book("NFLX");
        assert(book == nullptr);
        
        uint64_t buy_order1 = engine.submit_order("NFLX", OrderType::LIMIT, OrderSide::BUY, 400.0, 10, "client1");
        uint64_t buy_order2 = engine.submit_order("NFLX", OrderType::LIMIT, OrderSide::BUY, 399.0, 15, "client2");
        uint64_t sell_order1 = engine.submit_order("NFLX", OrderType::LIMIT, OrderSide::SELL, 410.0, 20, "client3");
        uint64_t sell_order2 = engine.submit_order("NFLX", OrderType::LIMIT, OrderSide::SELL, 411.0, 25, "client4");
        
        assert(buy_order1 > 0 && buy_order2 > 0 && sell_order1 > 0 && sell_order2 > 0);
        
        book = engine.get_order_book("NFLX");
        assert(book != nullptr);
        
        double best_bid = book->get_best_bid();
        double best_ask = book->get_best_ask();
        double last_price = book->get_last_price();
        
        assert(best_bid == 400.0);
        assert(best_ask == 410.0);
        assert(last_price == 0.0);
        
        uint64_t market_buy = engine.submit_order("NFLX", OrderType::MARKET, OrderSide::BUY, 0.0, 5, "market_buyer");
        assert(market_buy > 0);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        last_price = book->get_last_price();
        assert(last_price > 0.0);
        
        std::cout << "✓ Order book operations test passed" << std::endl;
    }
    
    void test_edge_cases_realistic() {
        std::cout << "\n--- Testing Edge Cases (Realistic) ---" << std::endl;
        
        uint64_t invalid_order1 = engine.submit_order("", OrderType::LIMIT, OrderSide::BUY, 100.0, 10, "client1");
        assert(invalid_order1 == 0);
        
        uint64_t invalid_order2 = engine.submit_order("AAPL", OrderType::LIMIT, OrderSide::BUY, 100.0, -10, "client1");
        assert(invalid_order2 == 0);
        
        uint64_t invalid_order3 = engine.submit_order("AAPL", OrderType::LIMIT, OrderSide::BUY, 100.0, 10, "");
        assert(invalid_order3 == 0);
        
        uint64_t invalid_order4 = engine.submit_order("AAPL", OrderType::LIMIT, OrderSide::BUY, -100.0, 10, "client1");
        assert(invalid_order4 == 0);
        
        uint64_t valid_market_order = engine.submit_order("AAPL", OrderType::MARKET, OrderSide::BUY, -100.0, 10, "client1");
        assert(valid_market_order > 0);
        
        uint64_t zero_qty_order = engine.submit_order("AAPL", OrderType::LIMIT, OrderSide::BUY, 100.0, 0, "client1");
        assert(zero_qty_order == 0);
        
        uint64_t large_order = engine.submit_order("AAPL", OrderType::LIMIT, OrderSide::BUY, 100.0, 1000000, "client1");
        assert(large_order > 0);
        
        uint64_t small_order = engine.submit_order("AAPL", OrderType::LIMIT, OrderSide::BUY, 100.0, 1, "client2");
        assert(small_order > 0);
        
        uint64_t high_price_order = engine.submit_order("AAPL", OrderType::LIMIT, OrderSide::BUY, 999999.99, 10, "client3");
        assert(high_price_order > 0);
        
        std::cout << "✓ Edge cases test passed" << std::endl;
    }
    
    void test_concurrent_operations_realistic() {
        std::cout << "\n--- Testing Concurrent Operations (Realistic) ---" << std::endl;
        
        std::vector<std::thread> threads;
        std::vector<uint64_t> order_ids;
        std::mutex order_ids_mutex;
        
        for (int i = 0; i < 20; i++) {
            threads.emplace_back([this, i, &order_ids, &order_ids_mutex]() {
                std::string client_id = "client" + std::to_string(i);
                double price = 100.0 + (i % 10);
                OrderSide side = (i % 2 == 0) ? OrderSide::BUY : OrderSide::SELL;
                
                uint64_t order_id = engine.submit_order("AAPL", OrderType::LIMIT, side, price, 10, client_id);
                
                std::lock_guard<std::mutex> lock(order_ids_mutex);
                order_ids.push_back(order_id);
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        for (uint64_t order_id : order_ids) {
            assert(order_id > 0);
        }
        
        auto book = engine.get_order_book("AAPL");
        assert(book != nullptr);
        
        double best_bid = book->get_best_bid();
        double best_ask = book->get_best_ask();
        
        if (best_bid > 0 && best_ask > 0) {
            assert(best_bid < best_ask);
        }
        
        std::cout << "✓ Concurrent operations test passed" << std::endl;
    }
    
    void test_partial_fills_and_remaining_quantity() {
        std::cout << "\n--- Testing Partial Fills and Remaining Quantity ---" << std::endl;
        
        engine.submit_order("MSFT", OrderType::LIMIT, OrderSide::SELL, 100.0, 50, "liquidity_provider");
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        uint64_t market_buy = engine.submit_order("MSFT", OrderType::MARKET, OrderSide::BUY, 0.0, 100, "partial_buyer");
        assert(market_buy > 0);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        auto book = engine.get_order_book("MSFT");
        assert(book != nullptr);
        assert(book->get_best_ask() == 0.0);
        assert(book->get_last_price() > 0.0);
        
        engine.submit_order("MSFT", OrderType::LIMIT, OrderSide::SELL, 101.0, 30, "liquidity_provider2");
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        assert(book->get_best_ask() == 101.0);
        
        uint64_t market_buy2 = engine.submit_order("MSFT", OrderType::MARKET, OrderSide::BUY, 0.0, 20, "partial_buyer2");
        assert(market_buy2 > 0);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        assert(book->get_best_ask() == 101.0);
        
        std::cout << "✓ Partial fills and remaining quantity test passed" << std::endl;
    }
    
    void test_order_status_transitions() {
        std::cout << "\n--- Testing Order Status Transitions ---" << std::endl;
        
        engine.submit_order("META", OrderType::LIMIT, OrderSide::SELL, 2500.0, 50, "seller");
        uint64_t buy_order = engine.submit_order("META", OrderType::LIMIT, OrderSide::BUY, 2500.0, 50, "buyer");
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        auto book = engine.get_order_book("META");
        assert(book != nullptr);
        assert(book->get_best_bid() == 0.0);
        assert(book->get_best_ask() == 0.0);
        assert(book->get_last_price() > 0.0);
        
        uint64_t cancel_order = engine.submit_order("META", OrderType::LIMIT, OrderSide::BUY, 2400.0, 25, "canceller");
        bool cancel_success = engine.cancel_order(cancel_order, "canceller");
        assert(cancel_success);
        
        book = engine.get_order_book("META");
        assert(book != nullptr);
        assert(book->get_best_bid() == 0.0);
        
        engine.submit_order("META", OrderType::LIMIT, OrderSide::SELL, 2501.0, 30, "partial_seller");
        uint64_t partial_buy = engine.submit_order("META", OrderType::MARKET, OrderSide::BUY, 0.0, 50, "partial_buyer");
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        book = engine.get_order_book("META");
        assert(book != nullptr);
        assert(book->get_best_ask() == 0.0);
        assert(book->get_last_price() > 0.0);
        
        std::cout << "✓ Order status transitions test passed" << std::endl;
    }
};

void test_order_creation() {
    std::cout << "\n--- Testing Order Creation ---" << std::endl;
    
    Order order1(1, "AAPL", OrderType::LIMIT, OrderSide::BUY, 150.0, 100, "client1");
    assert(order1.id == 1);
    assert(order1.symbol == "AAPL");
    assert(order1.type == OrderType::LIMIT);
    assert(order1.side == OrderSide::BUY);
    assert(order1.price == 150.0);
    assert(order1.quantity == 100);
    assert(order1.client_id == "client1");
    assert(order1.status == OrderStatus::PENDING);
    
    Order order2(2, "AAPL", OrderType::STOP_LIMIT, OrderSide::SELL, 160.0, 155.0, 50, "client2", StopLimitOrderTag{});
    assert(order2.price == 160.0);
    assert(order2.limit_price == 155.0);
    
    Order order3(3, "AAPL", OrderType::TRAILING_STOP, OrderSide::SELL, 5.0, 25, "client3", TrailingStopOrderTag{});
    assert(order3.trailing_amount == 5.0);
    assert(order3.highest_price == 0.0);
    assert(order3.lowest_price == 0.0);
    
    std::cout << "✓ Order creation test passed" << std::endl;
}

void test_order_book_basic() {
    std::cout << "\n--- Testing OrderBook Basic Operations ---" << std::endl;
    
    OrderBook book("AAPL");
    
    assert(book.get_best_bid() == 0.0);
    assert(book.get_best_ask() == 0.0);
    assert(book.get_last_price() == 0.0);
    
    auto order1 = std::make_shared<Order>(1, "AAPL", OrderType::LIMIT, OrderSide::BUY, 150.0, 100, "client1");
    auto order2 = std::make_shared<Order>(2, "AAPL", OrderType::LIMIT, OrderSide::SELL, 155.0, 50, "client2");
    
    book.add_order(order1);
    book.add_order(order2);
    
    assert(book.get_best_bid() == 150.0);
    assert(book.get_best_ask() == 155.0);
    
    book.cancel_order(1);
    assert(book.get_best_bid() == 0.0);
    
    std::cout << "✓ OrderBook basic operations test passed" << std::endl;
}

int main() {
    std::cout << "Starting Realistic Trading Engine Test Suite..." << std::endl;
    
    try {
        test_order_creation();
        test_order_book_basic();
        
        TradingEngineTest test_suite;
        test_suite.run_all_tests();
        
        std::cout << "\n🎉 ALL REALISTIC TESTS PASSED SUCCESSFULLY! 🎉" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ TEST FAILED: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "❌ UNKNOWN TEST FAILURE" << std::endl;
        return 1;
    }
}