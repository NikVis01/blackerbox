# Blackbox

Blackbox is a CLI tool for monitoring vLLM usage. It provides real-time insights and metrics for tracking the performance and resource utilization of your vLLM deployments.

## API Response Structure

The `/vram` endpoint returns a JSON object with the following structure:

```json
{
  "total_bytes": 42949672960,
  "used_bytes": 34561064960,
  "free_bytes": 8388608000,
  "reserved_bytes": 34561064960,
  "used_percent": 80.45,
  "active_blocks": 14401,
  "utilized_blocks": 0,
  "free_blocks": 14401,
  "atomic_allocations_bytes": 31299958784,
  "fragmentation_ratio": 0.8045,
  "processes": [
    {
      "pid": 131963,
      "name": "VLLM::EngineCor",
      "used_bytes": 31299958784,
      "reserved_bytes": 31299958784
    }
  ],
  "threads": [
    {
      "thread_id": 0,
      "allocated_bytes": 31299958784,
      "state": "active"
    }
  ],
  "blocks": [
    {
      "block_id": 0,
      "address": 0,
      "size": 2173952,
      "type": "kv_cache",
      "allocated": true,
      "utilized": false
    }
  ],
  "nsight_metrics": {
    "131963": {
      "atomic_operations": 0,
      "threads_per_block": 0,
      "occupancy": 0.0,
      "active_blocks": 0,
      "memory_throughput": 0,
      "dram_read_bytes": 0,
      "dram_write_bytes": 0,
      "available": false
    }
  }
}
```

### Field Descriptions

- **`total_bytes`**: Total GPU memory in bytes (from NVML)
- **`used_bytes`**: Currently used GPU memory (from NVML)
- **`free_bytes`**: Free GPU memory (from NVML)
- **`reserved_bytes`**: Reserved GPU memory (from NVML)
- **`used_percent`**: Memory usage percentage (0-100)
- **`active_blocks`**: Total allocated KV cache blocks (from vLLM)
- **`utilized_blocks`**: Blocks actively storing data (calculated from vLLM's `kv_cache_usage_perc`)
- **`free_blocks`**: Allocated but unused blocks (calculated: `active_blocks - utilized_blocks`)
- **`atomic_allocations_bytes`**: Sum of all process memory allocations (from NVML)
- **`fragmentation_ratio`**: Memory fragmentation (0-1, calculated)
- **`processes`**: Array of GPU processes with PID, name, and memory usage (from NVML)
- **`threads`**: Empty array (removed - was redundant 1:1 mapping of processes)
- **`blocks`**: Array of memory blocks with allocation and utilization status
  - **`block_id`**: Block identifier (0 to `active_blocks-1`)
  - **`address`**: Memory address (0 if unknown)
  - **`size`**: Block size in bytes (calculated from NVML process memory / num_blocks)
  - **`type`**: Block type ("kv_cache" for vLLM blocks)
  - **`allocated`**: Whether block is allocated (always `true` for vLLM blocks)
  - **`utilized`**: Whether block is actively storing data (from vLLM's `kv_cache_usage_perc`)
- **`nsight_metrics`**: Object keyed by PID containing Nsight Compute metrics
  - **`atomic_operations`**: Count of atomic operations (from Nsight Compute)
  - **`threads_per_block`**: CUDA threads per block (from Nsight Compute)
  - **`occupancy`**: GPU occupancy percentage (from Nsight Compute)
  - **`active_blocks`**: Active CUDA blocks (not parsed, always 0)
  - **`memory_throughput`**: Memory throughput (not parsed, always 0)
  - **`dram_read_bytes`**: DRAM read bytes (from Nsight Compute)
  - **`dram_write_bytes`**: DRAM write bytes (from Nsight Compute)
  - **`available`**: Whether Nsight Compute metrics are available
