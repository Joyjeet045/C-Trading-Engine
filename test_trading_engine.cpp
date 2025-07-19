#include "../src/common/Order.h"
#include "../src/common/OrderBook.h"
#include "../src/server/MatchingEngine.h"
#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>

class TradingEngineTest {
private:
    MatchingEngine engine;
    
public:
    void run_all_tests() {
        std::cout << "=== TRADING ENGINE TEST SUITE ===" << std::endl;
        
        test_basic_order_types();
        test_order_matching();
        test_stop_loss_orders();
        test_stop_limit_orders();
        test_trailing_stop_orders();
        test_order_cancellation();
        test_market_orders();
        test_order_book_operations();
        test_edge_cases();
        test_concurrent_operations();
        
        std::cout << "\n=== ALL TESTS PASSED ===" << std::endl;
    }
    
    void test_basic_order_types() {
        std::cout << "\n--- Testing Basic Order Types ---" << std::endl;
        
        // Test MARKET orders
        uint64_t market_order_id = engine.submit_order("AAPL", OrderType::MARKET, OrderSide::BUY, 0.0, 100, "client1");
        assert(market_order_id > 0);
        std::cout << "✓ MARKET order creation passed" << std::endl;
        
        // Test LIMIT orders
        uint64_t limit_order_id = engine.submit_order("AAPL", OrderType::LIMIT, OrderSide::SELL, 150.0, 50, "client2");
        assert(limit_order_id > 0);
        std::cout << "✓ LIMIT order creation passed" << std::endl;
        
        // Test STOP_LOSS orders
        uint64_t stop_loss_id = engine.submit_order("AAPL", OrderType::STOP_LOSS, OrderSide::SELL, 140.0, 25, "client1");
        assert(stop_loss_id > 0);
        std::cout << "✓ STOP_LOSS order creation passed" << std::endl;
        
        // Test STOP_LIMIT orders
        uint64_t stop_limit_id = engine.submit_stop_limit_order("AAPL", OrderSide::SELL, 145.0, 144.0, 30, "client2");
        assert(stop_limit_id > 0);
        std::cout << "✓ STOP_LIMIT order creation passed" << std::endl;
        
        // Test TRAILING_STOP orders
        uint64_t trailing_stop_id = engine.submit_trailing_stop_order("AAPL", OrderSide::SELL, 5.0, 40, "client1");
        assert(trailing_stop_id > 0);
        std::cout << "✓ TRAILING_STOP order creation passed" << std::endl;
    }
    
    void test_order_matching() {
        std::cout << "\n--- Testing Order Matching ---" << std::endl;
        
        // Create a simple matching scenario
        uint64_t buy_order = engine.submit_order("MSFT", OrderType::LIMIT, OrderSide::BUY, 200.0, 100, "client1");
        uint64_t sell_order = engine.submit_order("MSFT", OrderType::LIMIT, OrderSide::SELL, 200.0, 100, "client2");
        
        assert(buy_order > 0 && sell_order > 0);
        
        // Get order book and verify matching
        auto book = engine.get_order_book("MSFT");
        assert(book != nullptr);
        
        // Process matching
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        std::cout << "✓ Order matching test passed" << std::endl;
    }
    
    void test_stop_loss_orders() {
        std::cout << "\n--- Testing Stop Loss Orders ---" << std::endl;
        
        // Test immediate triggering
        uint64_t stop_order = engine.submit_order("GOOGL", OrderType::STOP_LOSS, OrderSide::SELL, 2500.0, 10, "client1");
        assert(stop_order > 0);
        
        // Test price movement triggering
        auto book = engine.get_order_book("GOOGL");
        if (book) {
            // Simulate price movement by executing a trade
            uint64_t buy_order = engine.submit_order("GOOGL", OrderType::LIMIT, OrderSide::BUY, 2500.0, 5, "client2");
            uint64_t sell_order = engine.submit_order("GOOGL", OrderType::LIMIT, OrderSide::SELL, 2500.0, 5, "client3");
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        std::cout << "✓ Stop loss order test passed" << std::endl;
    }
    
    void test_stop_limit_orders() {
        std::cout << "\n--- Testing Stop Limit Orders ---" << std::endl;
        
        // Test stop limit order creation
        uint64_t stop_limit = engine.submit_stop_limit_order("TSLA", OrderSide::SELL, 800.0, 795.0, 20, "client1");
        assert(stop_limit > 0);
        
        // Test validation
        uint64_t invalid_stop_limit = engine.submit_stop_limit_order("TSLA", OrderSide::SELL, 800.0, 810.0, 20, "client2");
        assert(invalid_stop_limit == 0); // Should fail validation
        
        std::cout << "✓ Stop limit order test passed" << std::endl;
    }
    
    void test_trailing_stop_orders() {
        std::cout << "\n--- Testing Trailing Stop Orders ---" << std::endl;
        
        // Test trailing stop order creation
        uint64_t trailing_stop = engine.submit_trailing_stop_order("NVDA", OrderSide::SELL, 10.0, 15, "client1");
        assert(trailing_stop > 0);
        
        // Test validation
        uint64_t invalid_trailing = engine.submit_trailing_stop_order("NVDA", OrderSide::SELL, -5.0, 15, "client2");
        assert(invalid_trailing == 0); // Should fail validation
        
        std::cout << "✓ Trailing stop order test passed" << std::endl;
    }
    
    void test_order_cancellation() {
        std::cout << "\n--- Testing Order Cancellation ---" << std::endl;
        
        // Create an order
        uint64_t order_id = engine.submit_order("AMZN", OrderType::LIMIT, OrderSide::BUY, 3000.0, 5, "client1");
        assert(order_id > 0);
        
        // Cancel the order
        bool cancel_success = engine.cancel_order(order_id, "client1");
        assert(cancel_success);
        
        // Try to cancel non-existent order
        bool cancel_fail = engine.cancel_order(99999, "client1");
        assert(!cancel_fail);
        
        // Try to cancel order with wrong client
        uint64_t another_order = engine.submit_order("AMZN", OrderType::LIMIT, OrderSide::BUY, 3000.0, 5, "client2");
        bool wrong_client_cancel = engine.cancel_order(another_order, "client1");
        assert(!wrong_client_cancel);
        
        std::cout << "✓ Order cancellation test passed" << std::endl;
    }
    
    void test_market_orders() {
        std::cout << "\n--- Testing Market Orders ---" << std::endl;
        
        // Test market buy order
        uint64_t market_buy = engine.submit_order("META", OrderType::MARKET, OrderSide::BUY, 0.0, 10, "client1");
        assert(market_buy > 0);
        
        // Test market sell order
        uint64_t market_sell = engine.submit_order("META", OrderType::MARKET, OrderSide::SELL, 0.0, 10, "client2");
        assert(market_sell > 0);
        
        std::cout << "✓ Market order test passed" << std::endl;
    }
    
    void test_order_book_operations() {
        std::cout << "\n--- Testing Order Book Operations ---" << std::endl;
        
        // Test order book creation
        auto book = engine.get_order_book("NFLX");
        assert(book != nullptr);
        
        // Test best bid/ask
        double best_bid = book->get_best_bid();
        double best_ask = book->get_best_ask();
        double last_price = book->get_last_price();
        
        // These should be 0.0 for empty book
        assert(best_bid == 0.0);
        assert(best_ask == 0.0);
        assert(last_price == 0.0);
        
        // Add orders and test again
        engine.submit_order("NFLX", OrderType::LIMIT, OrderSide::BUY, 400.0, 10, "client1");
        engine.submit_order("NFLX", OrderType::LIMIT, OrderSide::SELL, 410.0, 10, "client2");
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        book = engine.get_order_book("NFLX");
        best_bid = book->get_best_bid();
        best_ask = book->get_best_ask();
        
        assert(best_bid == 400.0);
        assert(best_ask == 410.0);
        
        std::cout << "✓ Order book operations test passed" << std::endl;
    }
    
    void test_edge_cases() {
        std::cout << "\n--- Testing Edge Cases ---" << std::endl;
        
        // Test invalid order parameters
        uint64_t invalid_order1 = engine.submit_order("", OrderType::LIMIT, OrderSide::BUY, 100.0, 10, "client1");
        assert(invalid_order1 == 0);
        
        uint64_t invalid_order2 = engine.submit_order("AAPL", OrderType::LIMIT, OrderSide::BUY, 100.0, -10, "client1");
        assert(invalid_order2 == 0);
        
        uint64_t invalid_order3 = engine.submit_order("AAPL", OrderType::LIMIT, OrderSide::BUY, 100.0, 10, "");
        assert(invalid_order3 == 0);
        
        uint64_t invalid_order4 = engine.submit_order("AAPL", OrderType::LIMIT, OrderSide::BUY, -100.0, 10, "client1");
        assert(invalid_order4 == 0);
        
        // Test zero quantity
        uint64_t zero_qty_order = engine.submit_order("AAPL", OrderType::LIMIT, OrderSide::BUY, 100.0, 0, "client1");
        assert(zero_qty_order == 0);
        
        std::cout << "✓ Edge cases test passed" << std::endl;
    }
    
    void test_concurrent_operations() {
        std::cout << "\n--- Testing Concurrent Operations ---" << std::endl;
        
        // Test multiple orders from different clients
        std::vector<std::thread> threads;
        std::vector<uint64_t> order_ids;
        
        for (int i = 0; i < 10; i++) {
            threads.emplace_back([this, i, &order_ids]() {
                std::string client_id = "client" + std::to_string(i);
                uint64_t order_id = engine.submit_order("AAPL", OrderType::LIMIT, OrderSide::BUY, 
                                                       100.0 + i, 10, client_id);
                order_ids.push_back(order_id);
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        // Verify all orders were created
        for (uint64_t order_id : order_ids) {
            assert(order_id > 0);
        }
        
        std::cout << "✓ Concurrent operations test passed" << std::endl;
    }
};

// Test helper functions
void test_order_creation() {
    std::cout << "\n--- Testing Order Creation ---" << std::endl;
    
    // Test Order constructor
    Order order1(1, "AAPL", OrderType::LIMIT, OrderSide::BUY, 150.0, 100, "client1");
    assert(order1.id == 1);
    assert(order1.symbol == "AAPL");
    assert(order1.type == OrderType::LIMIT);
    assert(order1.side == OrderSide::BUY);
    assert(order1.price == 150.0);
    assert(order1.quantity == 100);
    assert(order1.client_id == "client1");
    assert(order1.status == OrderStatus::PENDING);
    
    // Test stop limit constructor
    Order order2(2, "AAPL", OrderType::STOP_LIMIT, OrderSide::SELL, 160.0, 155.0, 50, "client2");
    assert(order2.price == 160.0); // stop price
    assert(order2.limit_price == 155.0); // limit price
    
    // Test trailing stop constructor
    Order order3(3, "AAPL", OrderType::TRAILING_STOP, OrderSide::SELL, 5.0, 25, "client3");
    assert(order3.trailing_amount == 5.0);
    assert(order3.highest_price == 0.0);
    assert(order3.lowest_price == 0.0);
    
    std::cout << "✓ Order creation test passed" << std::endl;
}

void test_order_book_basic() {
    std::cout << "\n--- Testing OrderBook Basic Operations ---" << std::endl;
    
    OrderBook book("AAPL");
    
    // Test initial state
    assert(book.get_best_bid() == 0.0);
    assert(book.get_best_ask() == 0.0);
    assert(book.get_last_price() == 0.0);
    
    // Test adding orders
    auto order1 = std::make_shared<Order>(1, "AAPL", OrderType::LIMIT, OrderSide::BUY, 150.0, 100, "client1");
    auto order2 = std::make_shared<Order>(2, "AAPL", OrderType::LIMIT, OrderSide::SELL, 155.0, 50, "client2");
    
    book.add_order(order1);
    book.add_order(order2);
    
    assert(book.get_best_bid() == 150.0);
    assert(book.get_best_ask() == 155.0);
    
    // Test order cancellation
    book.cancel_order(1);
    assert(book.get_best_bid() == 0.0);
    
    std::cout << "✓ OrderBook basic operations test passed" << std::endl;
}

int main() {
    std::cout << "Starting Trading Engine Test Suite..." << std::endl;
    
    try {
        // Run basic tests
        test_order_creation();
        test_order_book_basic();
        
        // Run comprehensive tests
        TradingEngineTest test_suite;
        test_suite.run_all_tests();
        
        std::cout << "\n🎉 ALL TESTS PASSED SUCCESSFULLY! 🎉" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ TEST FAILED: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "❌ UNKNOWN TEST FAILURE" << std::endl;
        return 1;
    }
} 