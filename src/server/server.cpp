#include "MatchingEngine.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <thread>
#include <cstring>
#include <unordered_map>
#include <mutex>
#include <chrono>

class TradingServer {
private:
    MatchingEngine engine;
    int server_fd;
    static const int PORT = 8080;
    std::unordered_map<std::string, int> active_sessions; // client_id -> client_fd
    std::mutex sessions_mutex;
    
public:
    TradingServer() : server_fd(-1) {}
    
    bool start() {
        server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd < 0) {
            std::cerr << "Failed to create socket" << std::endl;
            return false;
        }
        
        int opt = 1;
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(PORT);
        
        if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
            std::cerr << "Bind failed" << std::endl;
            return false;
        }
        
        if (listen(server_fd, 10) < 0) {
            std::cerr << "Listen failed" << std::endl;
            return false;
        }
        
        std::cout << "Trading server listening on port " << PORT << std::endl;
        
        while (true) {
            sockaddr_in client_address;
            socklen_t client_len = sizeof(client_address);
            int client_fd = accept(server_fd, (struct sockaddr*)&client_address, &client_len);
            
            if (client_fd < 0) {
                std::cerr << "Accept failed" << std::endl;
                continue;
            }
            
            std::thread client_thread(&TradingServer::handle_client, this, client_fd);
            client_thread.detach();
        }
        
        return true;
    }
    
    void handle_client(int client_fd) {
        char buffer[1024];
        std::string authenticated_client_id = "";
        
        while (true) {
            int bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
            if (bytes_read <= 0) break;
            
            buffer[bytes_read] = '\0';
            std::string response = process_message(std::string(buffer), client_fd, authenticated_client_id);
            
            if (response.find("LOGIN_SUCCESS") == 0) {
                size_t pos = response.find(":");
                if (pos != std::string::npos) {
                    authenticated_client_id = response.substr(pos + 1);
                    // Remove trailing newline if present
                    if (!authenticated_client_id.empty() && authenticated_client_id.back() == '\n') {
                        authenticated_client_id.pop_back();
                    }
                    std::cout << "DEBUG: Stored authenticated_client_id: [" << authenticated_client_id << "]" << std::endl;
                }
            }
            
            send(client_fd, response.c_str(), response.length(), 0);
        }
        
        if (!authenticated_client_id.empty()) {
            remove_session(authenticated_client_id);
        }
        close(client_fd);
    }
    
    bool add_session(const std::string& client_id, int client_fd) {
        std::lock_guard<std::mutex> lock(sessions_mutex);
        
        auto it = active_sessions.find(client_id);
        if (it != active_sessions.end()) {
            return false; // Client ID already in use
        }
        
        active_sessions[client_id] = client_fd;
        std::cout << "Client " << client_id << " logged in (FD: " << client_fd << ")" << std::endl;
        return true;
    }
    
    void remove_session(const std::string& client_id) {
        std::lock_guard<std::mutex> lock(sessions_mutex);
        
        auto it = active_sessions.find(client_id);
        if (it != active_sessions.end()) {
            std::cout << "Client " << client_id << " logged out (FD: " << it->second << ")" << std::endl;
            active_sessions.erase(it);
        }
    }
    
    bool is_authenticated(const std::string& client_id, int client_fd) {
        std::lock_guard<std::mutex> lock(sessions_mutex);
        
        auto it = active_sessions.find(client_id);
        return (it != active_sessions.end() && it->second == client_fd);
    }
    
    std::string process_message(const std::string& message, int client_fd, const std::string& authenticated_client_id) {
        std::istringstream iss(message);
        std::string command;
        iss >> command;
        
        if (command == "LOGIN") {
            std::string client_id;
            iss >> client_id;
            
            if (client_id.empty()) {
                return "LOGIN_FAILED:Invalid client ID\n";
            }
            
            if (add_session(client_id, client_fd)) {
                return "LOGIN_SUCCESS:" + client_id + "\n";
            } else {
                return "LOGIN_FAILED:Client ID already in use\n";
            }
        }
        else if (command == "ORDER") {
            if (authenticated_client_id.empty()) {
                return "ERROR:Not authenticated. Please LOGIN first.\n";
            }
            
            std::string symbol, type_str, side_str, client_id;
            double price, quantity;
            iss >> symbol >> type_str >> side_str >> price >> quantity >> client_id;
            
            std::cout << "DEBUG: Received client_id: [" << client_id << "], authenticated_client_id: [" << authenticated_client_id << "]" << std::endl;
            
            if (client_id != authenticated_client_id) {
                return "ERROR:Client ID mismatch. You can only place orders for your own account.\n";
            }
            
            OrderType type;
            if (type_str == "MARKET") {
                type = OrderType::MARKET;
            } else if (type_str == "LIMIT") {
                type = OrderType::LIMIT;
            } else if (type_str == "STOP_LOSS") {
                type = OrderType::STOP_LOSS;
            } else if (type_str == "STOP_LIMIT") {
                type = OrderType::STOP_LIMIT;
            } else if (type_str == "TRAILING_STOP") {
                type = OrderType::TRAILING_STOP;
            } else {
                return "ERROR:Invalid order type. Use MARKET, LIMIT, STOP_LOSS, STOP_LIMIT, or TRAILING_STOP.\n";
            }
            
            OrderSide side = (side_str == "BUY") ? OrderSide::BUY : OrderSide::SELL;
            
            uint64_t order_id = engine.submit_order(symbol, type, side, price, quantity, client_id);
            return "ORDER_ID:" + std::to_string(order_id) + "\n";
        }
        else if (command == "STOP_LIMIT_ORDER") {
            if (authenticated_client_id.empty()) {
                return "ERROR:Not authenticated. Please LOGIN first.\n";
            }
            
            std::string symbol, side_str, client_id;
            double stop_price, limit_price, quantity;
            iss >> symbol >> side_str >> stop_price >> limit_price >> quantity >> client_id;
            
            if (client_id != authenticated_client_id) {
                return "ERROR:Client ID mismatch. You can only place orders for your own account.\n";
            }
            
            OrderSide side = (side_str == "BUY") ? OrderSide::BUY : OrderSide::SELL;
            
            uint64_t order_id = engine.submit_stop_limit_order(symbol, side, stop_price, limit_price, quantity, client_id);
            return "ORDER_ID:" + std::to_string(order_id) + "\n";
        }
        else if (command == "TRAILING_STOP_ORDER") {
            if (authenticated_client_id.empty()) {
                return "ERROR:Not authenticated. Please LOGIN first.\n";
            }
            
            std::string symbol, side_str, client_id;
            double trailing_amount, quantity;
            iss >> symbol >> side_str >> trailing_amount >> quantity >> client_id;
            
            if (client_id != authenticated_client_id) {
                return "ERROR:Client ID mismatch. You can only place orders for your own account.\n";
            }
            
            OrderSide side = (side_str == "BUY") ? OrderSide::BUY : OrderSide::SELL;
            
            uint64_t order_id = engine.submit_trailing_stop_order(symbol, side, trailing_amount, quantity, client_id);
            return "ORDER_ID:" + std::to_string(order_id) + "\n";
        }
        else if (command == "VWAP_ORDER") {
            if (authenticated_client_id.empty()) {
                return "ERROR:Not authenticated. Please LOGIN first.\n";
            }
            
            std::string symbol, side_str, client_id;
            double target_vwap, quantity;
            int duration_minutes;
            iss >> symbol >> side_str >> target_vwap >> quantity >> duration_minutes >> client_id;
            
            if (client_id != authenticated_client_id) {
                return "ERROR:Client ID mismatch. You can only place orders for your own account.\n";
            }
            
            if (target_vwap <= 0 || quantity <= 0 || duration_minutes <= 0) {
                return "ERROR:Invalid VWAP parameters. Price, quantity, and duration must be positive.\n";
            }
            
            if (duration_minutes > 480) {
                return "ERROR:Duration cannot exceed 8 hours (480 minutes).\n";
            }
            
            OrderSide side = (side_str == "BUY") ? OrderSide::BUY : OrderSide::SELL;
            
            // Calculate start and end times
            auto now = std::chrono::steady_clock::now();
            auto start_time = now + std::chrono::seconds(1); // Start in 1 second
            auto end_time = now + std::chrono::minutes(duration_minutes);
            
            uint64_t order_id = engine.submit_vwap_order(symbol, side, target_vwap, quantity, start_time, end_time, client_id);
            
            if (order_id > 0) {
                return "VWAP_ORDER_ID:" + std::to_string(order_id) + "\n";
            } else {
                return "VWAP_ORDER_FAILED:Invalid parameters or insufficient liquidity\n";
            }
        }
        else if (command == "VWAP_STATUS") {
            if (authenticated_client_id.empty()) {
                return "ERROR:Not authenticated. Please LOGIN first.\n";
            }
            
            std::string symbol, client_id;
            iss >> symbol >> client_id;
            
            if (client_id != authenticated_client_id) {
                return "ERROR:Client ID mismatch. You can only check your own orders.\n";
            }
            
            auto active_vwap_orders = engine.get_active_vwap_orders();
            std::string response = "VWAP_STATUS:";
            
            bool found_orders = false;
            for (const auto& order : active_vwap_orders) {
                if (order->symbol == symbol && order->client_id == client_id) {
                    if (found_orders) response += "|";
                    response += "ID:" + std::to_string(order->id) + 
                               " SIDE:" + (order->side == OrderSide::BUY ? "BUY" : "SELL") +
                               " TARGET:" + std::to_string(order->target_vwap) +
                               " PROGRESS:" + std::to_string(order->filled_quantity) + "/" + std::to_string(order->quantity) +
                               " STATUS:" + std::to_string((int)order->status);
                    found_orders = true;
                }
            }
            
            if (!found_orders) {
                response += "NO_ACTIVE_VWAP_ORDERS";
            }
            
            return response + "\n";
        }
        else if (command == "CANCEL") {
            if (authenticated_client_id.empty()) {
                return "ERROR:Not authenticated. Please LOGIN first.\n";
            }
            
            uint64_t order_id;
            std::string client_id;
            iss >> order_id >> client_id;
            
            if (client_id != authenticated_client_id) {
                return "ERROR:Client ID mismatch. You can only cancel your own orders.\n";
            }
            
            bool success = engine.cancel_order(order_id, client_id);
            return success ? "CANCELLED\n" : "CANCEL_FAILED\n";
        }
        else if (command == "BOOK") {
            std::string symbol;
            iss >> symbol;
            
            auto book = engine.get_order_book(symbol);
            if (book) {
                return "BID:" + std::to_string(book->get_best_bid()) + 
                       " ASK:" + std::to_string(book->get_best_ask()) + 
                       " LAST:" + std::to_string(book->get_last_price()) + "\n";
            }
            return "BOOK_NOT_FOUND\n";
        }
        else if (command == "LOGOUT") {
            if (!authenticated_client_id.empty()) {
                remove_session(authenticated_client_id);
                return "LOGOUT_SUCCESS\n";
            }
            return "LOGOUT_FAILED:Not logged in\n";
        }
        
        return "UNKNOWN_COMMAND\n";
    }
    
    ~TradingServer() {
        if (server_fd >= 0) {
            close(server_fd);
        }
    }
};

int main() {
    TradingServer server;
    if (!server.start()) {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }
    return 0;
}