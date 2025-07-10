#include "../common/Order.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <thread>
#include <cstring>

class TradingClient {
private:
    int sock_fd;
    std::string client_id;
    static const int PORT = 8080;
    
public:
    TradingClient(const std::string& id) : sock_fd(-1), client_id(id) {}
    
    bool connect_to_server() {
        sock_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (sock_fd < 0) {
            std::cerr << "Failed to create socket" << std::endl;
            return false;
        }
        
        sockaddr_in server_address;
        server_address.sin_family = AF_INET;
        server_address.sin_port = htons(PORT);
        
        if (inet_pton(AF_INET, "127.0.0.1", &server_address.sin_addr) <= 0) {
            std::cerr << "Invalid address" << std::endl;
            return false;
        }
        
        if (connect(sock_fd, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
            std::cerr << "Connection failed" << std::endl;
            return false;
        }
        
        std::cout << "Connected to trading server as " << client_id << std::endl;
        return true;
    }
    
    void run() {
        std::string input;
        
        while (true) {
            std::cout << "\nCommands: ORDER, CANCEL, BOOK, QUIT" << std::endl;
            std::cout << "Enter command: ";
            std::getline(std::cin, input);
            
            if (input == "QUIT") break;
            
            if (input == "ORDER") {
                place_order();
            } else if (input == "CANCEL") {
                cancel_order();
            } else if (input == "BOOK") {
                get_book();
            } else {
                std::cout << "Unknown command" << std::endl;
            }
        }
    }
    
    void place_order() {
        std::string symbol, type, side;
        double price, quantity;
        
        std::cout << "Symbol: ";
        std::cin >> symbol;
        std::cout << "Type (MARKET/LIMIT/STOP_LOSS): ";
        std::cin >> type;
        std::cout << "Side (BUY/SELL): ";
        std::cin >> side;
        std::cout << "Price: ";
        std::cin >> price;
        std::cout << "Quantity: ";
        std::cin >> quantity;
        
        std::string message = "ORDER " + symbol + " " + type + " " + side + " " + 
                             std::to_string(price) + " " + std::to_string(quantity) + " " + client_id;
        
        send_message(message);
    }
    
    void cancel_order() {
        uint64_t order_id;
        std::cout << "Order ID to cancel: ";
        std::cin >> order_id;
        
        std::string message = "CANCEL " + std::to_string(order_id) + " " + client_id;
        send_message(message);
    }
    
    void get_book() {
        std::string symbol;
        std::cout << "Symbol: ";
        std::cin >> symbol;
        
        std::string message = "BOOK " + symbol;
        send_message(message);
    }
    
    void send_message(const std::string& message) {
        send(sock_fd, message.c_str(), message.length(), 0);
        
        char buffer[1024];
        int bytes_read = read(sock_fd, buffer, sizeof(buffer) - 1);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            std::cout << "Server response: " << buffer << std::endl;
        }
    }
    
    ~TradingClient() {
        if (sock_fd >= 0) {
            close(sock_fd);
        }
    }
};

int main() {
    std::string client_id;
    std::cout << "Enter client ID: ";
    std::cin >> client_id;
    std::cin.ignore(); // Clear newline
    
    TradingClient client(client_id);
    if (!client.connect_to_server()) {
        return 1;
    }
    
    client.run();
    return 0;
}
