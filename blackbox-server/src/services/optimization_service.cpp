#include "services/optimization_service.h"
#include "services/model_manager.h"
#include "services/hf_deploy.h"
#include "services/vram_tracker.h"
#include "utils/env_utils.h"
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <sstream>
#include <string>
#include <vector>
#include <fstream>
#include <algorithm>

void handleOptimizeRequest(http::request<http::string_body>& req, tcp::socket& socket) {
    OptimizationResult opt_result = optimizeModelAllocations();
    
    http::response<http::string_body> res;
    res.version(req.version());
    res.keep_alive(req.keep_alive());
    res.set(http::field::content_type, "application/json");
    
    if (!opt_result.optimized) {
        res.result(http::status::ok);
        res.body() = R"({"success":true,"optimized":false,"message":")" + opt_result.message + R"("})";
        res.prepare_payload();
        http::write(socket, res);
        return;
    }
    
    std::vector<std::string> restarted;
    for (const auto& container_name : opt_result.restarted_models) {
        auto models = listDeployedModels();
        std::string model_id;
        std::string gpu_type;
        double peak_usage = 0.0;
        
        for (const auto& m : models) {
            if (m.container_name == container_name) {
                model_id = m.model_id;
                gpu_type = m.gpu_type;
                peak_usage = m.peak_vram_usage_percent / 100.0;
                break;
            }
        }
        
        if (model_id.empty()) continue;
        
        spindownModel(container_name);
        
        std::string hf_token = getEnvValue("HF_TOKEN");
        if (gpu_type.empty()) gpu_type = detectGPUType();
        std::string config_path = getConfigPathForGPU(gpu_type);
        
        if (peak_usage < 0.1) peak_usage = 0.1;
        if (peak_usage > 0.95) peak_usage = 0.95;
        
        std::string temp_config = "/tmp/optimized_" + container_name + ".yaml";
        std::ifstream src(config_path);
        if (src.is_open()) {
            std::ofstream dst(temp_config);
            std::string line;
            while (std::getline(src, line)) {
                if (line.find("max_gpu_utilization:") != std::string::npos) {
                    dst << "  max_gpu_utilization: " << peak_usage << "\n";
                } else {
                    dst << line << "\n";
                }
            }
            src.close();
            dst.close();
        }
        
        DeployResponse deploy_res = deployHFModel(model_id, hf_token, 8000, gpu_type, temp_config);
        if (deploy_res.success) {
            restarted.push_back(container_name);
        }
    }
    
    std::ostringstream json;
    json << "{\"success\":true,\"optimized\":true,\"message\":\"Optimized " 
         << restarted.size() << " model(s)\",\"restarted_models\":[";
    for (size_t i = 0; i < restarted.size(); ++i) {
        if (i > 0) json << ",";
        json << "\"" << restarted[i] << "\"";
    }
    json << "]}";
    
    res.result(http::status::ok);
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

