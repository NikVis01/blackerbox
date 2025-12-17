#include "services/hf_deploy.h"
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

std::string generateDockerCommand(const std::string& model_id, const std::string& hf_token, int port) {
    std::ostringstream cmd;
    cmd << "docker run -d --runtime nvidia --gpus all "
        << "-v ~/.cache/huggingface:/root/.cache/huggingface "
        << "--env \"HF_TOKEN=" << hf_token << "\" "
        << "-p " << port << ":8000 "
        << "--ipc=host "
        << "--name vllm-" << std::regex_replace(model_id, std::regex("[^a-zA-Z0-9]"), "-") << " "
        << "vllm/vllm-openai:latest "
        << "--model " << model_id;
    return cmd.str();
}

DeployResponse deployHFModel(const std::string& model_id, const std::string& hf_token, int port) {
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
    
    if (!validateHFModel(model_id, token)) {
        response.message = "Failed to validate model. Check model_id and HF token.";
        return response;
    }
    
    std::string container_name = "vllm-" + std::regex_replace(model_id, std::regex("[^a-zA-Z0-9]"), "-");
    
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
    
    std::string docker_cmd = generateDockerCommand(model_id, token, port);
    
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
    
    response.success = true;
    response.container_id = container_id;
    response.message = absl::StrCat("Model deployed successfully. Container: ", container_id);
    
    return response;
}

