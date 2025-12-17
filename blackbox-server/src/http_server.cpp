#include "http_server.h"
#include "nvml_utils.h"
#include "json_serializer.h"
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio/connect.hpp>
#include <iostream>
#include <chrono>
#include <thread>
#include <sstream>
#include <string>

void handleStreamingRequest(tcp::socket& socket) {
    try {
        http::response<http::string_body> res;
        res.result(http::status::ok);
        res.set(http::field::content_type, "text/event-stream");
        res.set(http::field::cache_control, "no-cache");
        res.set(http::field::connection, "keep-alive");
        res.body() = "";
        res.prepare_payload();
        http::write(socket, res);
        
        while (true) {
            try {
                DetailedVRAMInfo info = getDetailedVRAMUsage();
                std::string json = createDetailedResponse(info);
                
                std::ostringstream event;
                event << "data: " << json << "\n\n";
                
                http::response<http::string_body> chunk;
                chunk.result(http::status::ok);
                chunk.set(http::field::content_type, "text/event-stream");
                chunk.body() = event.str();
                chunk.prepare_payload();
                
                http::write(socket, chunk);
                
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            } catch (const boost::system::system_error& e) {
                auto ec = e.code();
                if (ec == boost::asio::error::broken_pipe || 
                    ec == boost::asio::error::connection_reset ||
                    ec == boost::asio::error::eof ||
                    ec == boost::beast::http::error::end_of_stream ||
                    ec == boost::asio::error::operation_aborted ||
                    ec.category() == boost::asio::error::get_system_category()) {
                    break;
                }
                std::cerr << "Unexpected error in stream: " << e.what() << std::endl;
                break;
            } catch (const std::exception& e) {
                std::string err_msg = e.what();
                if (err_msg.find("end of stream") != std::string::npos ||
                    err_msg.find("end_of_stream") != std::string::npos ||
                    err_msg.find("Broken pipe") != std::string::npos ||
                    err_msg.find("Connection reset") != std::string::npos) {
                    break;
                }
                std::cerr << "Error in stream: " << e.what() << std::endl;
                break;
            }
        }
    } catch (...) {
    }
}

void handleRequest(http::request<http::string_body>& req, tcp::socket& socket) {
    std::string target = std::string(req.target());
    
    if (req.method() == http::verb::get) {
        if (target == "/vram" || target == "/vram/stream") {
            if (target == "/vram/stream") {
                handleStreamingRequest(socket);
                return;
            }
            
            DetailedVRAMInfo info = getDetailedVRAMUsage();
            std::string json = createDetailedResponse(info);
            
            http::response<http::string_body> res;
            res.version(req.version());
            res.keep_alive(req.keep_alive());
            res.result(http::status::ok);
            res.set(http::field::content_type, "application/json");
            res.body() = json;
            res.prepare_payload();
            
            try {
                http::write(socket, res);
            } catch (const boost::system::system_error& e) {
                auto ec = e.code();
                if (ec == boost::asio::error::broken_pipe || 
                    ec == boost::asio::error::connection_reset ||
                    ec == boost::asio::error::eof) {
                    return;
                }
                throw;
            }
            return;
        }
    }
    
    http::response<http::string_body> res;
    res.version(req.version());
    res.keep_alive(req.keep_alive());
    res.result(http::status::not_found);
    res.set(http::field::content_type, "text/plain");
    res.body() = "Not Found";
    res.prepare_payload();
    
    try {
        http::write(socket, res);
    } catch (const boost::system::system_error& e) {
        auto ec = e.code();
        if (ec == boost::asio::error::broken_pipe || 
            ec == boost::asio::error::connection_reset ||
            ec == boost::asio::error::eof) {
            return;
        }
        throw;
    }
}

void acceptConnections(tcp::acceptor& acceptor) {
    while (true) {
        try {
            tcp::socket socket(acceptor.get_executor());
            acceptor.accept(socket);
            
            beast::flat_buffer buffer;
            http::request<http::string_body> req;
            http::read(socket, buffer, req);
            handleRequest(req, socket);
        } catch (const boost::system::system_error& e) {
            auto ec = e.code();
            if (ec == boost::asio::error::broken_pipe || 
                ec == boost::asio::error::connection_reset ||
                ec == boost::asio::error::eof ||
                ec == boost::beast::http::error::end_of_stream ||
                ec == boost::asio::error::operation_aborted ||
                ec.category() == boost::asio::error::get_system_category()) {
                continue;
            }
            std::cerr << "Unexpected connection error: " << e.what() << std::endl;
        } catch (const std::exception& e) {
            std::string err_msg = e.what();
            if (err_msg.find("end of stream") != std::string::npos ||
                err_msg.find("end_of_stream") != std::string::npos ||
                err_msg.find("Broken pipe") != std::string::npos ||
                err_msg.find("Connection reset") != std::string::npos ||
                err_msg.find("Connection refused") != std::string::npos) {
                continue;
            }
            std::cerr << "Error handling request: " << e.what() << std::endl;
        } catch (...) {
            continue;
        }
    }
}

