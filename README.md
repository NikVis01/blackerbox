# Blackbox

Blackbox is a comprehensive monitoring solution for vLLM and GPU VRAM usage. It provides real-time insights, timeseries data collection, and interactive web dashboards for tracking the performance and resource utilization of your vLLM deployments.

## Components Overview

1. **CLI Monitor** (`blackbox-cli/`) - Terminal-based dashboard with live metrics
2. **VRAM Monitor** (`blackbox-server/py_script/vram_monitor.py`) - Command-line timeseries tracker
3. **Web Dashboard** (`blackbox-dashboard/`) - **NEW!** Interactive web interface with graphs and database storage

---

## Web Dashboard (NEW!)

The web dashboard provides a modern, browser-based interface for monitoring GPU memory with interactive charts and historical data analysis.

### Quick Start

```bash
# 1. Install dependencies
cd blackbox-dashboard
pip install -r requirements.txt

# 2. Start the API server (includes embedded data collection)
python api.py

# 3. Open browser to http://localhost:8001/
```

**Note:** Data collection is now embedded in the API server. When you add a node via the API, it automatically starts collecting data. No need to run a separate `data_collector.py` process!

### Features

- 📊 **Interactive Charts**: Real-time graphs for memory usage, processes, and fragmentation
- 💾 **Database Storage**: SQLite/PostgreSQL support for historical data
- 🔄 **Auto-refresh**: Configurable intervals (5s, 10s, 30s, 1m)
- ⏱️ **Time Ranges**: View data from 5 minutes to 24 hours
- 🔍 **Process Tracking**: Monitor individual GPU processes
- 📈 **Timeseries API**: REST endpoints for programmatic access
- 🎨 **Modern UI**: Gradient-styled responsive interface

See [`blackbox-dashboard/README.md`](blackbox-dashboard/README.md) for detailed documentation.

---

## Components

### VRAM Monitor (Python)

The VRAM Monitor (`blackbox-server/py_script/vram_monitor.py`) tracks GPU memory usage over time with timeseries data collection.

#### Features:
- Polls VRAM API every 5 seconds (configurable)
- Detects new processes, blocks, and threads by tracking IDs
- Stores timeseries history in JSON format (~/.blackbox_vram_history.json)
- Shows real-time statistics and trends
- Highlights new activity as it's detected
- Export history to CSV for analysis

#### Usage:

```bash
# Start monitoring with default settings (polls every 5 seconds)
./blackbox-server/py_script/vram_monitor.py

# Custom polling interval (e.g., 10 seconds)
./blackbox-server/py_script/vram_monitor.py --interval 10

# Custom API endpoint
./blackbox-server/py_script/vram_monitor.py --url http://localhost:8080/vram

# Custom history file location
./blackbox-server/py_script/vram_monitor.py --history-file /path/to/history.json

# Export history to CSV
./blackbox-server/py_script/vram_monitor.py --export-csv vram_history.csv

# Show statistics from collected history
./blackbox-server/py_script/vram_monitor.py --stats
```

#### Tracked Metrics:
- `total_bytes`, `used_bytes`, `free_bytes`, `reserved_bytes`
- `used_percent` - percentage of memory used
- `active_blocks`, `free_blocks` - memory block counts
- `atomic_allocations_bytes` - atomic allocation size
- `fragmentation_ratio` - memory fragmentation metric
- `processes` - running processes with memory usage
- `threads` - active threads with allocations
- `blocks` - detailed block information
- `nsight_metrics` - NVIDIA Nsight profiling data
- `vllm_metrics` - vLLM-specific metrics

#### New Request Detection:
The monitor tracks process IDs, block IDs, and thread IDs to detect new activity:
- **New Processes**: Highlights when new processes start using GPU memory
- **New Blocks**: Shows when new memory blocks are allocated
- **New Threads**: Tracks when new threads are created

#### History & Timeseries:
- Automatically saves snapshots to `~/.blackbox_vram_history.json`
- Maintains last 1000 snapshots
- Calculates trends (memory delta, process count changes, fragmentation)
- Shows statistics on exit (average, min, max usage)

---

## API Data Structure

DICT KEYS OF DATA OBJECT sent from host:

dict_keys(['total_bytes', 'used_bytes', 'free_bytes', 'reserved_bytes', 'used_percent', 'active_blocks', 'free_blocks', 'atomic_allocations_bytes', 'fragmentation_ratio', 'processes', 'threads', 'blocks', 'nsight_metrics', 'vllm_metrics'])