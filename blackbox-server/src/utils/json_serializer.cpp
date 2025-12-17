#include "utils/json_serializer.h"
#include <sstream>
#include <iomanip>

std::string createDetailedResponse(const DetailedVRAMInfo& info) {
    std::ostringstream oss;
    double usedPercent = info.total > 0 ? (100.0 * info.used / info.total) : 0.0;
    oss << R"({"total_bytes":)" << info.total
        << R"(,"used_bytes":)" << info.used
        << R"(,"free_bytes":)" << info.free
        << R"(,"reserved_bytes":)" << info.reserved
        << R"(,"used_percent":)" << std::fixed << std::setprecision(2) << usedPercent
        << R"(,"allocated_blocks":)" << info.allocated_blocks
        << R"(,"utilized_blocks":)" << info.utilized_blocks
        << R"(,"free_blocks":)" << info.free_blocks
        << R"(,"atomic_allocations_bytes":)" << info.atomic_allocations
        << R"(,"fragmentation_ratio":)" << std::fixed << std::setprecision(4) << info.fragmentation_ratio
        << R"(,"processes":[)";
    for (size_t i = 0; i < info.processes.size(); ++i) {
        if (i > 0) oss << ",";
        oss << R"({"pid":)" << info.processes[i].pid
            << R"(,"name":")" << info.processes[i].name << R"(")"
            << R"(,"used_bytes":)" << info.processes[i].used_bytes
            << R"(,"reserved_bytes":)" << info.processes[i].reserved_bytes << "}";
    }
    oss << R"(],"threads":[)";
    for (size_t i = 0; i < info.threads.size(); ++i) {
        if (i > 0) oss << ",";
        oss << R"({"thread_id":)" << info.threads[i].thread_id
            << R"(,"allocated_bytes":)" << info.threads[i].allocated_bytes
            << R"(,"state":")" << info.threads[i].state << R"("})";
    }
    oss << R"(],"blocks":[)";
    for (size_t i = 0; i < info.blocks.size(); ++i) {
        if (i > 0) oss << ",";
        oss << R"({"block_id":)" << info.blocks[i].block_id
            << R"(,"address":)" << info.blocks[i].address
            << R"(,"size":)" << info.blocks[i].size
            << R"(,"type":")" << info.blocks[i].type << R"(")"
            << R"(,"allocated":)" << (info.blocks[i].allocated ? "true" : "false")
            << R"(,"utilized":)" << (info.blocks[i].utilized ? "true" : "false") << "}";
    }
    oss << "]";
    
    oss << R"(,"nsight_metrics":{)";
    bool first_nsight = true;
    for (const auto& [pid, metrics] : info.nsight_metrics) {
        if (!first_nsight) oss << ",";
        first_nsight = false;
        oss << R"(")" << pid << R"(":{)"
            << R"("atomic_operations":)" << metrics.atomic_operations
            << R"(,"threads_per_block":)" << metrics.threads_per_block
            << R"(,"occupancy":)" << std::fixed << std::setprecision(4) << metrics.occupancy
            << R"(,"active_blocks":)" << metrics.active_blocks
            << R"(,"memory_throughput":)" << metrics.memory_throughput
            << R"(,"dram_read_bytes":)" << metrics.dram_read_bytes
            << R"(,"dram_write_bytes":)" << metrics.dram_write_bytes
            << R"(,"available":)" << (metrics.available ? "true" : "false")
            << "}";
    }
    oss << "}";
    oss << "}";
    return oss.str();
}

