package model

// VRAM snapshot from blackbox-server /vram endpoint
type Snapshot struct {
	TotalVRAMBytes      int64        `json:"total_vram_bytes"`      // Total VRAM available on the GPU
	AllocatedVRAMBytes  int64        `json:"allocated_vram_bytes"`  // What CUDA/NVML says is allocated (vLLM preallocating)
	UsedKVCacheBytes    int64        `json:"used_kv_cache_bytes"`   // Actual used KV cache (num_blocks * block_size * kv_cache_usage_perc)
	PrefixCacheHitRate  float64      `json:"prefix_cache_hit_rate"` // Prefix cache hit rate (0.0-100.0)
	Models              []ModelInfo  `json:"models"`                 // Per-model breakdown
}

type ModelInfo struct {
	ModelID            string `json:"model_id"`
	Port               int    `json:"port"`
	AllocatedVRAMBytes int64  `json:"allocated_vram_bytes"`
	UsedKVCacheBytes   int64  `json:"used_kv_cache_bytes"`
}
