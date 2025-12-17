#include "services/hf_deploy.h"
#include "services/model_manager.h"
#include "utils/env_utils.h"
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <fstream>
#include <iostream>
#include <string>
#include <regex>
#include <algorithm>
#include <absl/strings/str_cat.h>
#include <absl/strings/str_format.h>

bool validateHFModel(const std::string& model_id, const std::string& hf_token) {
    std::string cmd = absl::StrCat(
        "curl -s -H \"Authorization: Bearer ", hf_token, "\" ",
        "https://huggingface.co/api/models/", model_id, " 2>/dev/null"
    );
    
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return false;
    
    char buffer[4096];
    std::string result;
    while (fgets(buffer, sizeof(buffer), pipe)) {
        result += buffer;
    }
    int status = pclose(pipe);
    
    if (status != 0) return false;
    
    return result.find("\"id\":") != std::string::npos || 
           result.find("\"modelId\":") != std::string::npos;
}

double getMaxGPUUtilizationFromConfig(const std::string& config_path) {
    std::ifstream file(config_path);
    if (!file.is_open()) return 0.5;
    
    std::string line;
    while (std::getline(file, line)) {
        if (line.find("max_gpu_utilization:") != std::string::npos) {
            size_t pos = line.find(":");
            if (pos != std::string::npos) {
                try {
                    return std::stod(line.substr(pos + 1));
                } catch (...) {
                    return 0.5;
                }
            }
        }
    }
    return 0.5;
}

std::string getConfigPathForGPU(const std::string& gpu_type) {
    FILE* pwd_pipe = popen("pwd", "r");
    char pwd_buffer[512];
    std::string base_path = ".";
    if (pwd_pipe && fgets(pwd_buffer, sizeof(pwd_buffer), pwd_pipe)) {
        base_path = pwd_buffer;
        base_path.erase(base_path.find_last_not_of(" \t\n\r") + 1);
    }
    if (pwd_pipe) pclose(pwd_pipe);
    
    std::string config_file = base_path + "/blackbox-server/src/configs/" + gpu_type + ".yaml";
    FILE* check = fopen(config_file.c_str(), "r");
    if (check) {
        fclose(check);
        return config_file;
    }
    return base_path + "/blackbox-server/src/configs/T4.yaml";
}

std::string generateDockerCommand(const std::string& model_id, const std::string& hf_token, int port, const std::string& config_path) {
    std::ostringstream cmd;
    std::string container_name = "vllm-" + std::regex_replace(model_id, std::regex("[^a-zA-Z0-9]"), "-");
    
    std::string abs_config_path = config_path;
    if (config_path.find("/") != 0) {
        FILE* pwd_pipe = popen("pwd", "r");
        char pwd_buffer[512];
        if (pwd_pipe && fgets(pwd_buffer, sizeof(pwd_buffer), pwd_pipe)) {
            std::string pwd(pwd_buffer);
            pwd.erase(pwd.find_last_not_of(" \t\n\r") + 1);
            abs_config_path = pwd + "/" + config_path;
        }
        if (pwd_pipe) pclose(pwd_pipe);
    }
    
    cmd << "docker run -d --runtime nvidia --gpus all "
        << "-v ~/.cache/huggingface:/root/.cache/huggingface "
        << "-v " << abs_config_path << ":/app/config.yaml:ro "
        << "--env \"HF_TOKEN=" << hf_token << "\" "
        << "-p " << port << ":8000 "
        << "--ipc=host "
        << "--name " << container_name << " "
        << "vllm/vllm-openai:latest "
        << "--model " << model_id
        << " --backend-config /app/config.yaml";
    return cmd.str();
}

DeployResponse deployHFModel(const std::string& model_id, const std::string& hf_token, int port, const std::string& gpu_type, const std::string& custom_config_path) {
    DeployResponse response{false, "", "", port};
    
    if (model_id.empty()) {
        response.message = "Model ID is required";
        return response;
    }
    
    std::string token = hf_token;
    if (token.empty()) {
        token = getEnvValue("HF_TOKEN");
        if (token.empty()) {
            response.message = "HF token is required (provide in request or set HF_TOKEN in .env)";
            return response;
        }
    }
    
    if (!canDeployModel()) {
        int current = getDeployedModelCount();
        int max_allowed = getMaxConcurrentModels();
        response.message = absl::StrCat("Cannot deploy: ", current, " models already deployed (max: ", max_allowed, ")");
        return response;
    }
    
    if (!validateHFModel(model_id, token)) {
        response.message = "Failed to validate model. Check model_id and HF token.";
        return response;
    }
    
    std::string container_name = getContainerName(model_id);
    std::string detected_gpu = gpu_type.empty() ? detectGPUType() : gpu_type;
    std::string config_path = custom_config_path.empty() ? getConfigPathForGPU(detected_gpu) : custom_config_path;
    double max_gpu_util = getMaxGPUUtilizationFromConfig(config_path);
    
    std::string check_cmd = absl::StrCat("docker ps -a --filter name=", container_name, " --format {{.ID}}");
    FILE* check_pipe = popen(check_cmd.c_str(), "r");
    if (check_pipe) {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), check_pipe)) {
            std::string existing_id(buffer);
            if (!existing_id.empty() && existing_id.find_first_not_of(" \t\n\r") != std::string::npos) {
                existing_id.erase(existing_id.find_last_not_of(" \t\n\r") + 1);
                std::string stop_cmd = absl::StrCat("docker stop ", existing_id, " 2>/dev/null");
                std::string rm_cmd = absl::StrCat("docker rm ", existing_id, " 2>/dev/null");
                system(stop_cmd.c_str());
                system(rm_cmd.c_str());
            }
        }
        pclose(check_pipe);
    }
    
    std::string docker_cmd = generateDockerCommand(model_id, token, port, config_path);
    
    std::string script_path = absl::StrCat("/tmp/deploy_", container_name, ".sh");
    std::ofstream script(script_path);
    if (!script.is_open()) {
        response.message = "Failed to create deployment script";
        return response;
    }
    
    script << "#!/bin/bash\n";
    script << "set -e\n";
    script << docker_cmd << "\n";
    script.close();
    
    std::string chmod_cmd = absl::StrCat("chmod +x ", script_path);
    system(chmod_cmd.c_str());
    
    FILE* deploy_pipe = popen(absl::StrCat(script_path, " 2>&1").c_str(), "r");
    if (!deploy_pipe) {
        response.message = "Failed to execute deployment";
        return response;
    }
    
    char buffer[1024];
    std::string output;
    while (fgets(buffer, sizeof(buffer), deploy_pipe)) {
        output += buffer;
    }
    int status = pclose(deploy_pipe);
    
    if (status != 0) {
        response.message = "Deployment failed: " + output;
        return response;
    }
    
    std::string container_id = output;
    container_id.erase(container_id.find_last_not_of(" \t\n\r") + 1);
    
    if (container_id.length() < 12) {
        response.message = "Failed to get container ID";
        return response;
    }
    
    unsigned int pid = 0;
    FILE* pid_pipe = popen(absl::StrCat("docker inspect --format '{{.State.Pid}}' ", container_id, " 2>/dev/null").c_str(), "r");
    if (pid_pipe) {
        char pid_buffer[32];
        if (fgets(pid_buffer, sizeof(pid_buffer), pid_pipe)) {
            try {
                pid = std::stoi(pid_buffer);
            } catch (...) {}
        }
        pclose(pid_pipe);
    }
    
    registerModelDeployment(model_id, container_name, max_gpu_util, detected_gpu, pid);
    
    response.success = true;
    response.container_id = container_id;
    response.message = absl::StrCat("Model deployed successfully. Container: ", container_id);
    
    return response;
}

