#pragma once

#include "vram_types.h"
#include <vector>
#include <string>

struct ModelBlockData {
    std::string model_id;
    int port;
    unsigned int num_gpu_blocks;
    unsigned long long block_size;
    double kv_cache_usage_perc;
    double prefix_cache_hit_rate;
    bool available;
};

VLLMBlockData fetchVLLMBlockData();
std::vector<ModelBlockData> fetchPerModelBlockData();

