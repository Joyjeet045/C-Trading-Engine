#!/bin/bash

echo "🧪 TRADING ENGINE TEST SUITE"
echo "============================"

# Clean previous builds
echo "Cleaning previous builds..."
make clean

# Build the test suite
echo "Building test suite..."
make test

if [ $? -ne 0 ]; then
    echo "❌ Build failed!"
    exit 1
fi

echo "✅ Build successful!"

# Run the tests
echo ""
echo "Running tests..."
echo "================"

./bin/test_engine

if [ $? -eq 0 ]; then
    echo ""
    echo "🎉 ALL TESTS PASSED! 🎉"
    echo ""
    echo "Test Summary:"
    echo "✅ Basic order types (MARKET, LIMIT, STOP_LOSS, STOP_LIMIT, TRAILING_STOP)"
    echo "✅ Order matching logic"
    echo "✅ Stop loss order triggering"
    echo "✅ Stop limit order validation"
    echo "✅ Trailing stop order functionality"
    echo "✅ Order cancellation"
    echo "✅ Market order execution"
    echo "✅ Order book operations"
    echo "✅ Edge case handling"
    echo "✅ Concurrent operations"
    echo ""
    echo "The trading engine is ready for smart features implementation!"
else
    echo ""
    echo "❌ SOME TESTS FAILED!"
    echo "Please check the output above for details."
    exit 1
fi 