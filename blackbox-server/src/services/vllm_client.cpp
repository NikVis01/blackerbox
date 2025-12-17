#include "services/vllm_client.h"
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <iostream>
#include <string>

VLLMBlockData fetchVLLMBlockData() {
    VLLMBlockData data{0, 0, 0.0, false};
    
    FILE* curl = popen("curl -s --max-time 1 http://localhost:8000/metrics 2>/dev/null", "r");
    if (!curl) return data;
    
    char line[4096];
    bool found_kv_usage = false;
    while (fgets(line, sizeof(line), curl)) {
        std::string line_str(line);
        
        size_t pos = line_str.find("vllm:cache_config_info");
        if (pos != std::string::npos) {
            size_t num_blocks_pos = line_str.find("num_gpu_blocks=\"", pos);
            if (num_blocks_pos != std::string::npos) {
                num_blocks_pos += 16;
                size_t num_blocks_end = line_str.find("\"", num_blocks_pos);
                if (num_blocks_end != std::string::npos) {
                    std::string value_str = line_str.substr(num_blocks_pos, num_blocks_end - num_blocks_pos);
                    std::string digits;
                    for (char c : value_str) {
                        if (std::isdigit(c)) digits += c;
                    }
                    if (!digits.empty()) {
                        data.num_gpu_blocks = std::stoi(digits);
                    }
                }
            }
        }
        
        size_t kv_usage_pos = line_str.find("vllm:kv_cache_usage_perc");
        if (kv_usage_pos != std::string::npos) {
            found_kv_usage = true;
            size_t brace_end = line_str.find("}", kv_usage_pos);
            if (brace_end != std::string::npos) {
                size_t value_start = line_str.find_first_not_of(" \t", brace_end + 1);
                if (value_start != std::string::npos) {
                    size_t value_end = line_str.find_first_of(" \n\r", value_start);
                    if (value_end == std::string::npos) {
                        value_end = line_str.length();
                    }
                    if (value_end > value_start) {
                        std::string value_str = line_str.substr(value_start, value_end - value_start);
                        try {
                            data.kv_cache_usage_perc = std::stod(value_str);
                            if (data.kv_cache_usage_perc < 0.0) data.kv_cache_usage_perc = 0.0;
                            if (data.kv_cache_usage_perc > 1.0) data.kv_cache_usage_perc = 1.0;
                        } catch (const std::exception&) {
                            data.kv_cache_usage_perc = 0.0;
                        }
                    }
                }
            }
        }
    }
    
    pclose(curl);
    
    if (data.num_gpu_blocks > 0) {
        data.available = true;
        data.block_size = 16 * 1024;
    }
    
    return data;
}

