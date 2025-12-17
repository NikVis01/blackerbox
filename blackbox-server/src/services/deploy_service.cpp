#include "services/deploy_service.h"
#include "utils/json_parser.h"
#include "utils/env_utils.h"
#include "services/hf_deploy.h"
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <sstream>
#include <string>

void handleDeployRequest(http::request<http::string_body>& req, tcp::socket& socket) {
    std::string body = req.body();
    std::string model_id = parseJSONField(body, "model_id");
    std::string hf_token = parseJSONField(body, "hf_token");
    int port = parseJSONInt(body, "port", 8000);
    
    http::response<http::string_body> res;
    res.version(req.version());
    res.keep_alive(req.keep_alive());
    res.set(http::field::content_type, "application/json");
    
    if (model_id.empty()) {
        res.result(http::status::bad_request);
        res.body() = R"({"success":false,"message":"model_id is required"})";
        res.prepare_payload();
        http::write(socket, res);
        return;
    }
    
    if (hf_token.empty()) {
        hf_token = getEnvValue("HF_TOKEN");
        if (hf_token.empty()) {
            res.result(http::status::bad_request);
            res.body() = R"({"success":false,"message":"hf_token is required (provide in request or set HF_TOKEN in .env)"})";
            res.prepare_payload();
            http::write(socket, res);
            return;
        }
    }
    
    std::string gpu_type = getEnvValue("GPU_TYPE", "");
    DeployResponse deploy_result = deployHFModel(model_id, hf_token, port, gpu_type);
    
    std::ostringstream json;
    if (deploy_result.success) {
        res.result(http::status::ok);
        json << "{\"success\":true,\"message\":\"" << deploy_result.message
             << "\",\"container_id\":\"" << deploy_result.container_id
             << "\",\"port\":" << deploy_result.port << "}";
    } else {
        res.result(http::status::internal_server_error);
        json << "{\"success\":false,\"message\":\"" << deploy_result.message << "\"}";
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

