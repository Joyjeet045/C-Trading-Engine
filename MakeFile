CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -pthread

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

.PHONY: all clean server client

all: server client

server: $(SERVER_TARGET)

client: $(CLIENT_TARGET)

$(SERVER_TARGET): $(SERVER_OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(CLIENT_TARGET): $(CLIENT_OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -rf $(OBJDIR) $(BINDIR)

run-server: server
	./$(SERVER_TARGET)

run-client: client
	./$(CLIENT_TARGET)
