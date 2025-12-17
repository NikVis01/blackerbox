#include "services/model_manager.h"
#include "services/vram_tracker.h"
#include "services/hf_deploy.h"
#include "utils/env_utils.h"
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>
#include <regex>
#include <algorithm>
#include <map>
#include <deque>
#include <numeric>
#include <absl/strings/str_cat.h>

static std::map<std::string, ModelMetrics> model_metrics;
static const int MAX_SAMPLES = 100;

int getMaxConcurrentModels() {
    std::string max_str = getEnvValue("MAX_CONCURRENT_MODELS", "3");
    try {
        int max = std::stoi(max_str);
        return max > 0 ? max : 3;
    } catch (...) {
        return 3;
    }
}

std::string getContainerName(const std::string& model_id) {
    return "vllm-" + std::regex_replace(model_id, std::regex("[^a-zA-Z0-9]"), "-");
}

std::vector<DeployedModel> listDeployedModels() {
    std::vector<DeployedModel> models;
    
    FILE* pipe = popen("docker ps -a --filter name=vllm- --format '{{.ID}}|{{.Names}}|{{.Status}}|{{.Ports}}' 2>/dev/null", "r");
    if (!pipe) return models;
    
    char line[512];
    while (fgets(line, sizeof(line), pipe)) {
        std::string line_str(line);
        if (line_str.empty()) continue;
        
        std::istringstream iss(line_str);
        std::string container_id, name, status, ports;
        
        std::getline(iss, container_id, '|');
        std::getline(iss, name, '|');
        std::getline(iss, status, '|');
        std::getline(iss, ports);
        
        if (name.find("vllm-") != 0) continue;
        
        container_id.erase(container_id.find_last_not_of(" \t\n\r") + 1);
        name.erase(name.find_last_not_of(" \t\n\r") + 1);
        status.erase(status.find_last_not_of(" \t\n\r") + 1);
        
        std::string model_id = name.substr(5);
        
        int port = 8000;
        size_t port_pos = ports.find(":8000");
        if (port_pos != std::string::npos) {
            size_t start = ports.rfind(":", port_pos - 1);
            if (start != std::string::npos) {
                try {
                    port = std::stoi(ports.substr(start + 1, port_pos - start - 1));
                } catch (...) {}
            }
        }
        
        DeployedModel model;
        model.model_id = model_id;
        model.container_id = container_id;
        model.container_name = name;
        model.port = port;
        model.running = status.find("Up") == 0;
        
        models.push_back(model);
    }
    pclose(pipe);
    
    return models;
}

std::vector<DeployedModel> listDeployedModels() {
    std::vector<DeployedModel> models = listDeployedModelsBase();

bool isModelDeployed(const std::string& model_id) {
    std::string container_name = getContainerName(model_id);
    std::string cmd = absl::StrCat("docker ps -a --filter name=", container_name, " --format {{.ID}}");
    
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return false;
    
    char buffer[128];
    bool exists = false;
    if (fgets(buffer, sizeof(buffer), pipe)) {
        std::string id(buffer);
        id.erase(id.find_last_not_of(" \t\n\r") + 1);
        exists = !id.empty() && id.length() >= 12;
    }
    pclose(pipe);
    
    return exists;
}

bool canDeployModel() {
    int current = getDeployedModelCount();
    int max_allowed = getMaxConcurrentModels();
    return current < max_allowed;
}

bool spindownModel(const std::string& model_id_or_container) {
    std::string container_name = model_id_or_container;
    
    if (container_name.find("vllm-") != 0) {
        container_name = getContainerName(model_id_or_container);
    }
    
    unregisterModel(container_name);
    
    std::string stop_cmd = absl::StrCat("docker stop ", container_name, " 2>/dev/null");
    std::string rm_cmd = absl::StrCat("docker rm ", container_name, " 2>/dev/null");
    
    int stop_result = system(stop_cmd.c_str());
    int rm_result = system(rm_cmd.c_str());
    
    return (stop_result == 0 || rm_result == 0);
}

std::string detectGPUType() {
    FILE* pipe = popen("nvidia-smi --query-gpu=name --format=csv,noheader 2>/dev/null | head -1", "r");
    if (!pipe) return "T4";
    
    char buffer[256];
    std::string gpu_name;
    if (fgets(buffer, sizeof(buffer), pipe)) {
        gpu_name = buffer;
        gpu_name.erase(gpu_name.find_last_not_of(" \t\n\r") + 1);
    }
    pclose(pipe);
    
    if (gpu_name.find("A100") != std::string::npos) return "A100";
    if (gpu_name.find("H100") != std::string::npos) return "H100";
    if (gpu_name.find("L40") != std::string::npos) return "L40";
    if (gpu_name.find("T4") != std::string::npos) return "T4";
    
    return "T4";
}

void registerModelDeployment(const std::string& model_id, const std::string& container_name,
                            double configured_max_gpu_utilization, const std::string& gpu_type, unsigned int pid) {
    ModelMetrics metrics;
    metrics.configured_max_utilization = configured_max_gpu_utilization;
    metrics.gpu_type = gpu_type;
    metrics.pid = pid;
    metrics.peak_usage = 0.0;
    model_metrics[container_name] = metrics;
}

void unregisterModel(const std::string& container_name) {
    model_metrics.erase(container_name);
}

void updateModelVRAMUsage(const std::string& container_name, double vram_percent) {
    if (model_metrics.find(container_name) == model_metrics.end()) return;
    
    auto& metrics = model_metrics[container_name];
    metrics.vram_samples.push_back(vram_percent);
    if (metrics.vram_samples.size() > MAX_SAMPLES) {
        metrics.vram_samples.pop_front();
    }
    
    if (vram_percent > metrics.peak_usage) {
        metrics.peak_usage = vram_percent;
    }
}


OptimizationResult optimizeModelAllocations() {
    OptimizationResult result{false, {}, ""};
    std::vector<std::string> to_restart;
    
    for (const auto& [container_name, metrics] : model_metrics) {
        if (metrics.vram_samples.size() < 10) continue;
        
        double avg = std::accumulate(metrics.vram_samples.begin(), metrics.vram_samples.end(), 0.0) / metrics.vram_samples.size();
        double threshold = metrics.configured_max_utilization * 100.0 * 0.7;
        
        if (avg < threshold && metrics.peak_usage > 0) {
            to_restart.push_back(container_name);
        }
    }
    
    if (to_restart.empty()) {
        result.message = "No models need optimization";
        return result;
    }
    
    result.optimized = true;
    result.restarted_models = to_restart;
    result.message = absl::StrCat("Optimizing ", to_restart.size(), " model(s)");
    
    return result;
}

