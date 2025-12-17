#pragma once

#include <string>

struct DeployResponse {
    bool success;
    std::string message;
    std::string container_id;
    int port;
};

DeployResponse deployHFModel(const std::string& model_id, const std::string& hf_token = "", int port = 8000);
bool validateHFModel(const std::string& model_id, const std::string& hf_token);
std::string generateDockerCommand(const std::string& model_id, const std::string& hf_token, int port);

