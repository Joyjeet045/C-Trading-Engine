CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -pthread -I.

SRCDIR = src
OBJDIR = obj
BINDIR = bin

# Create directories
$(shell mkdir -p $(OBJDIR) $(BINDIR))

# Server
SERVER_SOURCES = $(SRCDIR)/server/server.cpp $(SRCDIR)/server/MatchingEngine.cpp $(SRCDIR)/common/OrderBook.cpp
SERVER_OBJECTS = $(SERVER_SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
SERVER_TARGET = $(BINDIR)/server

# Client
CLIENT_SOURCES = $(SRCDIR)/client/client.cpp
CLIENT_OBJECTS = $(CLIENT_SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
CLIENT_TARGET = $(BINDIR)/client

# Test
TEST_SOURCES = test_trading_engine.cpp $(SRCDIR)/server/MatchingEngine.cpp $(SRCDIR)/common/OrderBook.cpp
TEST_OBJECTS = $(TEST_SOURCES:%.cpp=$(OBJDIR)/%.o)
TEST_TARGET = $(BINDIR)/test_engine

.PHONY: all clean server client test

all: server client test

server: $(SERVER_TARGET)

client: $(CLIENT_TARGET)

test: $(TEST_TARGET)

$(SERVER_TARGET): $(SERVER_OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(CLIENT_TARGET): $(CLIENT_OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(TEST_TARGET): $(TEST_OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(OBJDIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(OBJDIR)/test_trading_engine.o: test_trading_engine.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -DTEST_BUILD -c -o $@ $<

clean:
	rm -rf $(OBJDIR) $(BINDIR)

run-server: server
	./$(SERVER_TARGET)

run-client: client
	./$(CLIENT_TARGET)

run-test: test
	./$(TEST_TARGET)
