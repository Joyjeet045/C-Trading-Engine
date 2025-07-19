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
    bool authenticated;
    static const int PORT = 8080;
    
public:
    TradingClient(const std::string& id) : sock_fd(-1), client_id(id), authenticated(false) {}
    
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
        
        std::cout << "Connected to trading server" << std::endl;
        return true;
    }
    
    bool login() {
        std::string message = "LOGIN " + client_id;
        std::string response = send_message(message);
        
        if (response.find("LOGIN_SUCCESS") == 0) {
            authenticated = true;
            std::cout << "Successfully logged in as " << client_id << std::endl;
            return true;
        } else {
            std::cout << "Login failed: " << response << std::endl;
            return false;
        }
    }
    
    void logout() {
        if (authenticated) {
            send_message("LOGOUT");
            authenticated = false;
            std::cout << "Logged out" << std::endl;
        }
    }
    
    void run() {
        if (!login()) {
            std::cout << "Failed to authenticate. Exiting." << std::endl;
            return;
        }
        
        std::string input;
        
        while (true) {
            std::cout << "\nCommands: ORDER, STOP_LIMIT_ORDER, TRAILING_STOP_ORDER, VWAP_ORDER, VWAP_STATUS, CANCEL, BOOK, LOGOUT, QUIT" << std::endl;
            std::cout << "Enter command: ";
            std::getline(std::cin, input);
            
            if (input == "QUIT") {
                logout();
                break;
            }
            
            if (input == "ORDER") {
                place_order();
            } else if (input == "STOP_LIMIT_ORDER") {
                place_stop_limit_order();
            } else if (input == "TRAILING_STOP_ORDER") {
                place_trailing_stop_order();
            } else if (input == "VWAP_ORDER") {
                place_vwap_order();
            } else if (input == "VWAP_STATUS") {
                get_vwap_status();
            } else if (input == "CANCEL") {
                cancel_order();
            } else if (input == "BOOK") {
                get_book();
            } else if (input == "LOGOUT") {
                logout();
                break;
            } else {
                std::cout << "Unknown command" << std::endl;
            }
        }
    }
    
    void place_order() {
        if (!authenticated) {
            std::cout << "Not authenticated. Please login first." << std::endl;
            return;
        }
        
        std::string symbol, type, side;
        double price, quantity;
        
        std::cout << "Symbol: ";
        std::cin >> symbol;
        std::cout << "Type (MARKET/LIMIT/STOP_LOSS/STOP_LIMIT/TRAILING_STOP): ";
        std::cin >> type;
        std::cout << "Side (BUY/SELL): ";
        std::cin >> side;
        std::cout << "Price: ";
        std::cin >> price;
        std::cout << "Quantity: ";
        std::cin >> quantity;
        std::cin.ignore(); // Clear input buffer
        
        std::string message = "ORDER " + symbol + " " + type + " " + side + " " + 
                             std::to_string(price) + " " + std::to_string(quantity) + " " + client_id;
        
        send_message(message);
    }
    
    void place_stop_limit_order() {
        if (!authenticated) {
            std::cout << "Not authenticated. Please login first." << std::endl;
            return;
        }
        
        std::string symbol, side;
        double stop_price, limit_price, quantity;
        
        std::cout << "Symbol: ";
        std::cin >> symbol;
        std::cout << "Side (BUY/SELL): ";
        std::cin >> side;
        std::cout << "Stop Price: ";
        std::cin >> stop_price;
        std::cout << "Limit Price: ";
        std::cin >> limit_price;
        std::cout << "Quantity: ";
        std::cin >> quantity;
        std::cin.ignore();
        
        std::string message = "STOP_LIMIT_ORDER " + symbol + " " + side + " " + 
                             std::to_string(stop_price) + " " + std::to_string(limit_price) + " " + 
                             std::to_string(quantity) + " " + client_id;
        
        send_message(message);
    }
    
    void place_trailing_stop_order() {
        if (!authenticated) {
            std::cout << "Not authenticated. Please login first." << std::endl;
            return;
        }
        
        std::string symbol, side;
        double trailing_amount, quantity;
        
        std::cout << "Symbol: ";
        std::cin >> symbol;
        std::cout << "Side (BUY/SELL): ";
        std::cin >> side;
        std::cout << "Trailing Amount ($): ";
        std::cin >> trailing_amount;
        std::cout << "Quantity: ";
        std::cin >> quantity;
        std::cin.ignore(); 
        
        std::string message = "TRAILING_STOP_ORDER " + symbol + " " + side + " " + 
                             std::to_string(trailing_amount) + " " + std::to_string(quantity) + " " + client_id;
        
        send_message(message);
    }
    
    void place_vwap_order() {
        if (!authenticated) {
            std::cout << "Not authenticated. Please login first." << std::endl;
            return;
        }
        
        std::string symbol, side;
        double target_vwap, quantity;
        int duration_minutes;
        
        std::cout << "=== VWAP Order ===" << std::endl;
        std::cout << "VWAP orders execute over time to achieve the target average price." << std::endl;
        std::cout << "Symbol: ";
        std::cin >> symbol;
        std::cout << "Side (BUY/SELL): ";
        std::cin >> side;
        std::cout << "Target VWAP Price: ";
        std::cin >> target_vwap;
        std::cout << "Total Quantity: ";
        std::cin >> quantity;
        std::cout << "Duration (minutes): ";
        std::cin >> duration_minutes;
        std::cin.ignore();
        
        // Validate inputs
        if (target_vwap <= 0 || quantity <= 0 || duration_minutes <= 0) {
            std::cout << "Error: Invalid parameters. Price, quantity, and duration must be positive." << std::endl;
            return;
        }
        
        if (duration_minutes > 480) { // 8 hours max
            std::cout << "Error: Duration cannot exceed 8 hours (480 minutes)." << std::endl;
            return;
        }
        
        std::string message = "VWAP_ORDER " + symbol + " " + side + " " + 
                             std::to_string(target_vwap) + " " + std::to_string(quantity) + " " + 
                             std::to_string(duration_minutes) + " " + client_id;
        
        std::cout << "Submitting VWAP order..." << std::endl;
        std::string response = send_message(message);
        
        if (response.find("VWAP_ORDER_ID:") == 0) {
            std::string order_id = response.substr(14); // Remove "VWAP_ORDER_ID:"
            if (!order_id.empty() && order_id.back() == '\n') {
                order_id.pop_back();
            }
            std::cout << "✓ VWAP order submitted successfully!" << std::endl;
            std::cout << "  Order ID: " << order_id << std::endl;
            std::cout << "  Symbol: " << symbol << std::endl;
            std::cout << "  Side: " << side << std::endl;
            std::cout << "  Target VWAP: $" << target_vwap << std::endl;
            std::cout << "  Quantity: " << quantity << std::endl;
            std::cout << "  Duration: " << duration_minutes << " minutes" << std::endl;
            std::cout << "  Use VWAP_STATUS to monitor progress" << std::endl;
        } else {
            std::cout << "✗ VWAP order failed: " << response << std::endl;
        }
    }
    
    void get_vwap_status() {
        if (!authenticated) {
            std::cout << "Not authenticated. Please login first." << std::endl;
            return;
        }
        
        std::string symbol;
        std::cout << "Symbol: ";
        std::cin >> symbol;
        std::cin.ignore();
        
        std::string message = "VWAP_STATUS " + symbol + " " + client_id;
        std::string response = send_message(message);
        
        // Parse and display VWAP status response
        if (response.find("VWAP_STATUS:") == 0) {
            std::string status_data = response.substr(12); // Remove "VWAP_STATUS:"
            
            if (status_data.find("NO_ACTIVE_VWAP_ORDERS") != std::string::npos) {
                std::cout << "No active VWAP orders found for " << symbol << std::endl;
            } else {
                std::cout << "\n=== VWAP Orders for " << symbol << " ===" << std::endl;
                
                // Parse multiple orders separated by "|"
                size_t pos = 0;
                while ((pos = status_data.find("|")) != std::string::npos) {
                    std::string order_info = status_data.substr(0, pos);
                    display_vwap_order_info(order_info);
                    status_data.erase(0, pos + 1);
                }
                
                // Display the last order
                if (!status_data.empty()) {
                    display_vwap_order_info(status_data);
                }
            }
        } else {
            std::cout << "Error getting VWAP status: " << response << std::endl;
        }
    }
    
    void display_vwap_order_info(const std::string& order_info) {
        std::cout << "Order Info: " << order_info << std::endl;
        
        // Parse individual fields
        std::istringstream iss(order_info);
        std::string field;
        
        while (iss >> field) {
            if (field.find("ID:") == 0) {
                std::cout << "  Order ID: " << field.substr(3) << std::endl;
            } else if (field.find("SIDE:") == 0) {
                std::cout << "  Side: " << field.substr(5) << std::endl;
            } else if (field.find("TARGET:") == 0) {
                std::cout << "  Target VWAP: $" << field.substr(7) << std::endl;
            } else if (field.find("PROGRESS:") == 0) {
                std::cout << "  Progress: " << field.substr(9) << std::endl;
            } else if (field.find("STATUS:") == 0) {
                int status = std::stoi(field.substr(7));
                std::string status_str = (status == 0) ? "PENDING" : 
                                       (status == 1) ? "PARTIAL_FILLED" : 
                                       (status == 2) ? "FILLED" : 
                                       (status == 3) ? "CANCELLED" : "UNKNOWN";
                std::cout << "  Status: " << status_str << std::endl;
            }
        }
        std::cout << std::endl;
    }
    
    void cancel_order() {
        if (!authenticated) {
            std::cout << "Not authenticated. Please login first." << std::endl;
            return;
        }
        
        uint64_t order_id;
        std::cout << "Order ID to cancel: ";
        std::cin >> order_id;
        std::cin.ignore(); 
        
        std::string message = "CANCEL " + std::to_string(order_id) + " " + client_id;
        send_message(message);
    }
    
    void get_book() {
        std::string symbol;
        std::cout << "Symbol: ";
        std::cin >> symbol;
        std::cin.ignore();
        
        std::string message = "BOOK " + symbol;
        send_message(message);
    }

    std::string send_message(const std::string& message) {
        std::cout << "DEBUG: Sending message: [" << message << "]" << std::endl;
        send(sock_fd, message.c_str(), message.length(), 0);
        
        char buffer[1024];
        int bytes_read = read(sock_fd, buffer, sizeof(buffer) - 1);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            std::string response(buffer);
            std::cout << "Server response: " << response;
            return response;
        }
        return "";
    }
    
    ~TradingClient() {
        if (authenticated) {
            logout();
        }
        if (sock_fd >= 0) {
            close(sock_fd);
        }
    }
};

int main() {
    std::string client_id;
    std::cout << "Enter client ID: ";
    std::cin >> client_id;
    std::cin.ignore(); 
    
    TradingClient client(client_id);
    if (!client.connect_to_server()) {
        return 1;
    }
    
    client.run();
    return 0;
}
