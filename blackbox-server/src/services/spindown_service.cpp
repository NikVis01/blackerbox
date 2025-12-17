#include "services/spindown_service.h"
#include "services/model_manager.h"
#include "utils/json_parser.h"
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <sstream>
#include <string>

void handleSpindownRequest(http::request<http::string_body>& req, tcp::socket& socket) {
    std::string body = req.body();
    std::string model_id = parseJSONField(body, "model_id");
    std::string container_id = parseJSONField(body, "container_id");
    
    http::response<http::string_body> res;
    res.version(req.version());
    res.keep_alive(req.keep_alive());
    res.set(http::field::content_type, "application/json");
    
    std::string target = model_id.empty() ? container_id : model_id;
    if (target.empty()) {
        res.result(http::status::bad_request);
        res.body() = R"({"success":false,"message":"model_id or container_id is required"})";
        res.prepare_payload();
        http::write(socket, res);
        return;
    }
    
    bool success = spindownModel(target);
    
    std::ostringstream json;
    if (success) {
        res.result(http::status::ok);
        json << "{\"success\":true,\"message\":\"Model spindown successful\",\"target\":\"" << target << "\"}";
    } else {
        res.result(http::status::internal_server_error);
        json << "{\"success\":false,\"message\":\"Failed to spindown model: " << target << "\"}";
    }
    res.body() = json.str();
    
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

void handleListModelsRequest(http::request<http::string_body>& req, tcp::socket& socket) {
    std::vector<DeployedModel> models = listDeployedModels();
    int max_allowed = getMaxConcurrentModels();
    int running = 0;
    for (const auto& m : models) {
        if (m.running) running++;
    }
    
    http::response<http::string_body> res;
    res.version(req.version());
    res.keep_alive(req.keep_alive());
    res.result(http::status::ok);
    res.set(http::field::content_type, "application/json");
    
    std::ostringstream json;
    json << "{\"total\":" << models.size()
         << ",\"running\":" << running
         << ",\"max_allowed\":" << max_allowed
         << ",\"models\":[";
    
    for (size_t i = 0; i < models.size(); ++i) {
        if (i > 0) json << ",";
        json << "{\"model_id\":\"" << models[i].model_id
             << "\",\"container_id\":\"" << models[i].container_id
             << "\",\"container_name\":\"" << models[i].container_name
             << "\",\"port\":" << models[i].port
             << ",\"running\":" << (models[i].running ? "true" : "false") << "}";
    }
    
    json << "]}";
    res.body() = json.str();
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

