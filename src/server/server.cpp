#include "MatchingEngine.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <thread>
#include <cstring>

class TradingServer {
private:
    MatchingEngine engine;
    int server_fd;
    static const int PORT = 8080;
    
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
        
        while (true) {
            int bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
            if (bytes_read <= 0) break;
            
            buffer[bytes_read] = '\0';
            std::string response = process_message(std::string(buffer));
            
            send(client_fd, response.c_str(), response.length(), 0);
        }
        
        close(client_fd);
    }
    
    std::string process_message(const std::string& message) {
        std::istringstream iss(message);
        std::string command;
        iss >> command;
        
        if (command == "ORDER") {
            std::string symbol, type_str, side_str, client_id;
            double price, quantity;
            iss >> symbol >> type_str >> side_str >> price >> quantity >> client_id;
            
            OrderType type = (type_str == "MARKET") ? OrderType::MARKET :
                           (type_str == "LIMIT") ? OrderType::LIMIT : OrderType::STOP_LOSS;
            OrderSide side = (side_str == "BUY") ? OrderSide::BUY : OrderSide::SELL;
            
            uint64_t order_id = engine.submit_order(symbol, type, side, price, quantity, client_id);
            return "ORDER_ID:" + std::to_string(order_id) + "\n";
        }
        else if (command == "CANCEL") {
            uint64_t order_id;
            std::string client_id;
            iss >> order_id >> client_id;
            
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