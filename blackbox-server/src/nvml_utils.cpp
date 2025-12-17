#include "nvml_utils.h"
#include "vllm_client.h"
#include "nsight_utils.h"
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <algorithm>
#include <absl/strings/str_cat.h>
#ifdef NVML_AVAILABLE
#include <nvml.h>
#endif

#ifdef NVML_AVAILABLE
static nvmlDevice_t g_device = nullptr;
#else
static void* g_device = nullptr;
#endif
static bool g_nvml_initialized = false;

bool initNVML() {
    if (g_nvml_initialized) return true;
#ifdef NVML_AVAILABLE
    nvmlReturn_t result = nvmlInit();
    if (result != NVML_SUCCESS) {
        std::cerr << "[NVML] Initialization failed (error code: " << result << ")" << std::endl;
        if (result == NVML_ERROR_DRIVER_NOT_LOADED) {
            std::cerr << "[NVML] Driver not loaded. Try: sudo modprobe nvidia" << std::endl;
        } else if (result == NVML_ERROR_LIBRARY_NOT_FOUND) {
            std::cerr << "[NVML] Library not found. Install: sudo apt install -y nvidia-utils-535" << std::endl;
        } else if (result == NVML_ERROR_NO_PERMISSION) {
            std::cerr << "[NVML] Permission denied. Try running as root or add user to video group" << std::endl;
        } else {
            std::cerr << "[NVML] Check: 1) NVIDIA drivers installed? 2) GPU present? 3) nvidia-smi works?" << std::endl;
            std::cerr << "[NVML] If 'Driver/library version mismatch': Reboot or reinstall drivers" << std::endl;
        }
        return false;
    }
    
    unsigned int deviceCount = 0;
    result = nvmlDeviceGetCount(&deviceCount);
    if (result != NVML_SUCCESS) {
        std::cerr << "[NVML] Failed to get device count (error code: " << result << ")" << std::endl;
        nvmlShutdown();
        return false;
    }
    
    if (deviceCount == 0) {
        std::cerr << "[NVML] No GPU devices found" << std::endl;
        nvmlShutdown();
        return false;
    }
    
    std::cout << "[NVML] Found " << deviceCount << " GPU device(s)" << std::endl;
    
    result = nvmlDeviceGetHandleByIndex(0, &g_device);
    if (result != NVML_SUCCESS) {
        std::cerr << "[NVML] Failed to get device handle (error code: " << result << ")" << std::endl;
        nvmlShutdown();
        return false;
    }
    
    g_nvml_initialized = true;
    std::cout << "[NVML] Initialized successfully" << std::endl;
    return true;
#else
    std::cerr << "[NVML] NVML not available (compiled without NVML support)" << std::endl;
    std::cerr << "[NVML] Install: sudo apt install -y libnvidia-ml-dev" << std::endl;
    return false;
#endif
}

void shutdownNVML() {
    if (g_nvml_initialized) {
#ifdef NVML_AVAILABLE
        nvmlShutdown();
#endif
        g_nvml_initialized = false;
        g_device = nullptr;
    }
}

DetailedVRAMInfo getDetailedVRAMUsage() {
    DetailedVRAMInfo detailed = {0, 0, 0, 0, {}, {}, {}, 0, 0, 0, 0ULL, 0.0, {}};
    if (!initNVML()) {
        return detailed;
    }
#ifdef NVML_AVAILABLE
    nvmlMemory_t memory;
    if (nvmlDeviceGetMemoryInfo(g_device, &memory) == NVML_SUCCESS) {
        detailed.total = memory.total;
        detailed.used = memory.used;
        detailed.free = memory.free;
        detailed.reserved = memory.used;
    }

    unsigned int processCount = 64;
    nvmlProcessInfo_t processes[64];
    unsigned long long total_atomic_allocations = 0;
    
    if (nvmlDeviceGetComputeRunningProcesses(g_device, &processCount, processes) == NVML_SUCCESS) {
        for (unsigned int i = 0; i < processCount; ++i) {
            ProcessMemory pm;
            pm.pid = processes[i].pid;
            pm.used_bytes = processes[i].usedGpuMemory;
            pm.reserved_bytes = processes[i].usedGpuMemory;
            
            total_atomic_allocations += processes[i].usedGpuMemory;
            
            char name[256] = {0};
            FILE* fp = fopen(absl::StrCat("/proc/", pm.pid, "/comm").c_str(), "r");
            if (fp) {
                if (fgets(name, sizeof(name), fp)) {
                    pm.name = name;
                    if (!pm.name.empty() && pm.name.back() == '\n') {
                        pm.name.pop_back();
                    }
                } else {
                    pm.name = "unknown";
                }
                fclose(fp);
            } else {
                pm.name = "unknown";
            }
            detailed.processes.push_back(pm);
            
            NsightMetrics nsight = getNsightMetrics(pm.pid);
            if (nsight.available) {
                detailed.nsight_metrics[pm.pid] = nsight;
            }
        }
    }

    VLLMBlockData vllm_blocks = fetchVLLMBlockData();
    if (vllm_blocks.available) {
        detailed.allocated_blocks = vllm_blocks.num_gpu_blocks;
        
        unsigned long long calculated_block_size = vllm_blocks.block_size;
        for (const auto& pm : detailed.processes) {
            if (pm.name.find("python") != std::string::npos || 
                pm.name.find("vllm") != std::string::npos ||
                pm.name.find("VLLM") != std::string::npos) {
                if (vllm_blocks.num_gpu_blocks > 0 && pm.used_bytes > 0) {
                    calculated_block_size = pm.used_bytes / vllm_blocks.num_gpu_blocks;
                }
                break;
            }
        }
        
        for (unsigned int i = 0; i < vllm_blocks.num_gpu_blocks; ++i) {
            MemoryBlock block;
            block.block_id = i;
            block.address = 0;
            block.size = calculated_block_size;
            block.type = "kv_cache";
            block.allocated = true;
            block.utilized = false;
            detailed.blocks.push_back(block);
        }
    } else {
        detailed.allocated_blocks = 0;
        detailed.utilized_blocks = 0;
        detailed.free_blocks = 0;
    }
    
    unsigned int utilized_count = 0;
    if (!detailed.blocks.empty() && vllm_blocks.available) {
        unsigned int actual_utilized = static_cast<unsigned int>(
            vllm_blocks.num_gpu_blocks * vllm_blocks.kv_cache_usage_perc + 0.5
        );
        
        actual_utilized = std::min(actual_utilized, detailed.allocated_blocks);
        
        for (size_t j = 0; j < detailed.blocks.size() && j < actual_utilized; ++j) {
            detailed.blocks[j].utilized = true;
            utilized_count++;
        }
        
        detailed.utilized_blocks = utilized_count;
    } else {
        detailed.utilized_blocks = 0;
    }
    
    if (detailed.allocated_blocks > 0) {
        detailed.free_blocks = detailed.allocated_blocks - detailed.utilized_blocks;
    }

    detailed.atomic_allocations = total_atomic_allocations > 0 ? total_atomic_allocations : detailed.used;

    detailed.fragmentation_ratio = detailed.total > 0 ? 
        (1.0 - (double)detailed.free / detailed.total) : 0.0;
#endif
    return detailed;
}

