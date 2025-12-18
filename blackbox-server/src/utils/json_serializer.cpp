#include "utils/json_serializer.h"
#include <sstream>
#include <iomanip>

std::string createDetailedResponse(const DetailedVRAMInfo& info) {
    std::ostringstream oss;
    // Simplified response: total VRAM, allocated VRAM, used KV cache bytes, prefix cache hit rate, and per-model breakdown
    oss << R"({"total_vram_bytes":)" << info.total
        << R"(,"allocated_vram_bytes":)" << info.used
        << R"(,"used_kv_cache_bytes":)" << info.used_kv_cache_bytes
        << R"(,"prefix_cache_hit_rate":)" << std::fixed << std::setprecision(2) << info.prefix_cache_hit_rate
        << R"(,"models":[)";
    
    for (size_t i = 0; i < info.models.size(); ++i) {
        if (i > 0) oss << ",";
        const auto& model = info.models[i];
        oss << R"({"model_id":")" << model.model_id << R"(")"
            << R"(,"port":)" << model.port
            << R"(,"allocated_vram_bytes":)" << model.allocated_vram_bytes
            << R"(,"used_kv_cache_bytes":)" << model.used_kv_cache_bytes
            << "}";
    }
    
    oss << "]}";
    return oss.str();
}

