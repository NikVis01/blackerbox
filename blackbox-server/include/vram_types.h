#pragma once

#include <string>
#include <vector>
#include <map>

struct MemoryBlock {
    unsigned long long address;
    unsigned long long size;
    std::string type;
    int block_id;
    bool allocated;
    bool utilized;
};

struct ProcessMemory {
    unsigned int pid;
    std::string name;
    unsigned long long used_bytes;
    unsigned long long reserved_bytes;
};

struct ThreadInfo {
    int thread_id;
    unsigned long long allocated_bytes;
    std::string state;
};

struct NsightMetrics {
    unsigned long long atomic_operations;
    unsigned long long threads_per_block;
    double occupancy;
    unsigned long long active_blocks;
    unsigned long long memory_throughput;
    unsigned long long dram_read_bytes;
    unsigned long long dram_write_bytes;
    bool available;
};

struct DetailedVRAMInfo {
    unsigned long long total;
    unsigned long long used;
    unsigned long long free;
    unsigned long long reserved;
    std::vector<MemoryBlock> blocks;
    std::vector<ProcessMemory> processes;
    std::vector<ThreadInfo> threads;
    unsigned int allocated_blocks;
    unsigned int utilized_blocks;
    unsigned int free_blocks;
    unsigned long long atomic_allocations;
    double fragmentation_ratio;
    std::map<unsigned int, NsightMetrics> nsight_metrics;
};

struct VLLMBlockData {
    unsigned int num_gpu_blocks;
    unsigned long long block_size;
    double kv_cache_usage_perc;
    bool available;
};

