#include "http_server.h"
#include "nvml_utils.h"
#include <iostream>
#include <stdexcept>

int main(int argc, char* argv[]) {
    try {
        int port = 6767;
        if (argc > 1) port = std::stoi(argv[1]);

        net::io_context ioc;
        tcp::endpoint endpoint(tcp::v4(), port);
        tcp::acceptor acceptor(ioc, endpoint);
        
        std::cout << "VRAM monitor server listening on " 
                  << endpoint.address().to_string() << ":" << port << std::endl;
        
        initNVML();
        acceptConnections(acceptor);
    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        shutdownNVML();
        return 1;
    }
    shutdownNVML();
    return 0;
}
