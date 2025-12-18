# SSE Stream API Documentation

## Endpoint
```
GET /vram/stream
```

## Response Format

**Content-Type:** `text/event-stream`  
**Connection:** Keep-alive (long-lived connection)

## Event Format

Each event is sent as:
```
data: <JSON_OBJECT>\n\n
```

The JSON object follows this structure:

```json
{
  "total_vram_bytes": 85899345920,
  "allocated_vram_bytes": 78571110400,
  "used_kv_cache_bytes": 0,
  "prefix_cache_hit_rate": 73.45,
  "models": [
    {
      "model_id": "Qwen-Qwen3-0-6B",
      "port": 8000,
      "allocated_vram_bytes": 78571110400,
      "used_kv_cache_bytes": 0
    }
  ]
}
```

## Field Types

| Field | Type | Description |
|-------|------|-------------|
| `total_vram_bytes` | `int64` | Total GPU VRAM in bytes |
| `allocated_vram_bytes` | `int64` | Allocated VRAM in bytes |
| `used_kv_cache_bytes` | `int64` | Used KV cache in bytes |
| `prefix_cache_hit_rate` | `float64` | Prefix cache hit rate (0.0-100.0) |
| `models` | `[]ModelInfo` | Array of deployed models |

### ModelInfo Structure

| Field | Type | Description |
|-------|------|-------------|
| `model_id` | `string` | Model identifier |
| `port` | `int` | vLLM server port |
| `allocated_vram_bytes` | `int64` | VRAM allocated to this model |
| `used_kv_cache_bytes` | `int64` | KV cache used by this model |

## Update Frequency

Events are sent approximately every **500ms**.

## Go Implementation Example

```go
type Snapshot struct {
    TotalVRAMBytes     int64       `json:"total_vram_bytes"`
    AllocatedVRAMBytes int64       `json:"allocated_vram_bytes"`
    UsedKVCacheBytes   int64       `json:"used_kv_cache_bytes"`
    PrefixCacheHitRate float64     `json:"prefix_cache_hit_rate"`
    Models             []ModelInfo `json:"models"`
}

type ModelInfo struct {
    ModelID            string `json:"model_id"`
    Port               int    `json:"port"`
    AllocatedVRAMBytes int64  `json:"allocated_vram_bytes"`
    UsedKVCacheBytes   int64  `json:"used_kv_cache_bytes"`
}

// Reading SSE stream
reader := bufio.NewReader(resp.Body)
for {
    line, err := reader.ReadString('\n')
    if err != nil {
        break
    }
    
    line = strings.TrimSpace(line)
    if strings.HasPrefix(line, "data: ") {
        data := strings.TrimSpace(line[6:])
        var snap Snapshot
        if err := json.Unmarshal([]byte(data), &snap); err == nil {
            // Process snapshot
        }
    }
}
```

## Notes

- Events are separated by `\n\n` (two newlines)
- Empty lines between events are normal
- Connection stays open until client disconnects
- Server sends events continuously every ~500ms

