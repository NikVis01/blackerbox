#include "utils/env_utils.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <algorithm>

static std::map<std::string, std::string> env_cache;
static bool env_loaded = false;

std::map<std::string, std::string> loadEnvFile(const std::string& path) {
    std::map<std::string, std::string> env;
    
    std::ifstream file(path);
    if (!file.is_open()) {
        return env;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        size_t pos = line.find('=');
        if (pos == std::string::npos) {
            continue;
        }
        
        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);
        
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        
        if (value.length() >= 2 && value[0] == '"' && value[value.length() - 1] == '"') {
            value = value.substr(1, value.length() - 2);
        }
        
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);
        
        if (!key.empty()) {
            env[key] = value;
        }
    }
    
    return env;
}

std::string getEnvValue(const std::string& key, const std::string& default_val) {
    if (!env_loaded) {
        env_cache = loadEnvFile(".env");
        const char* home = std::getenv("HOME");
        if (home) {
            std::string home_env = std::string(home) + "/.env";
            auto home_cache = loadEnvFile(home_env);
            env_cache.insert(home_cache.begin(), home_cache.end());
        }
        env_loaded = true;
    }
    
    const char* env_val = std::getenv(key.c_str());
    if (env_val) {
        return std::string(env_val);
    }
    
    auto it = env_cache.find(key);
    if (it != env_cache.end()) {
        return it->second;
    }
    
    return default_val;
}

bool hasEnvKey(const std::string& key) {
    if (!env_loaded) {
        getEnvValue(key);
    }
    
    if (std::getenv(key.c_str())) {
        return true;
    }
    
    return env_cache.find(key) != env_cache.end();
}

