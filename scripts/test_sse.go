package main

import (
	"bufio"
	"context"
	"encoding/json"
	"flag"
	"fmt"
	"io"
	"net/http"
	"os"
	"os/signal"
	"strings"
	"syscall"
	"time"
)

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

func main() {
	var baseURL string
	flag.StringVar(&baseURL, "url", "http://localhost:6767", "Base URL of the blackbox server")
	flag.Parse()

	streamURL := baseURL + "/vram/stream"
	fmt.Printf("Connecting to SSE stream: %s\n", streamURL)
	fmt.Println("Press Ctrl+C to stop")

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	// Handle interrupt signal
	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, os.Interrupt, syscall.SIGTERM)
	go func() {
		<-sigChan
		fmt.Println("\nShutting down...")
		cancel()
	}()

	req, err := http.NewRequestWithContext(ctx, http.MethodGet, streamURL, nil)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Failed to create request: %v\n", err)
		os.Exit(1)
	}

	// For SSE, we want keep-alive connection, not close
	// Don't set Connection: close header for SSE streams
	req.Header.Set("Accept", "text/event-stream")

	// Create a client with no timeout for streaming
	transport := &http.Transport{
		DisableKeepAlives: false, // Keep connection alive for SSE
	}
	client := &http.Client{
		Timeout:   0, // No timeout for streaming
		Transport: transport,
	}

	resp, err := client.Do(req)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Request failed: %v\n", err)
		os.Exit(1)
	}
	defer resp.Body.Close()

	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		fmt.Fprintf(os.Stderr, "Server returned %s\n", resp.Status)
		os.Exit(1)
	}

	fmt.Printf("Connected! Status: %s\n", resp.Status)
	fmt.Printf("Content-Type: %s\n", resp.Header.Get("Content-Type"))
	fmt.Println("Reading SSE stream...")
	fmt.Println(strings.Repeat("=", 80))

	// Read SSE stream - read raw bytes first to see what we get
	reader := bufio.NewReader(resp.Body)
	snapshotCount := 0
	lineCount := 0

	// Try reading a chunk first to see if there's any data
	buf := make([]byte, 4096)
	n, err := reader.Read(buf)
	if err != nil && err != io.EOF {
		fmt.Fprintf(os.Stderr, "Initial read error: %v\n", err)
	}
	if n > 0 {
		fmt.Printf("[DEBUG] Initial read: %d bytes\n", n)
		fmt.Printf("[DEBUG] First 200 bytes: %q\n", string(buf[:min(n, 200)]))
		// Put it back for proper line-by-line reading
		reader = bufio.NewReader(io.MultiReader(strings.NewReader(string(buf[:n])), resp.Body))
	}

	for {
		select {
		case <-ctx.Done():
			fmt.Println("\nStream closed by context")
			return
		default:
		}

		// Read line by line
		line, err := reader.ReadString('\n')
		if err != nil {
			if err == io.EOF {
				// Check if we got any data at all
				if lineCount == 0 && snapshotCount == 0 {
					fmt.Printf("\n[%s] Stream ended immediately (EOF) - no data received\n",
						time.Now().Format("15:04:05.000"))
					fmt.Println("This might indicate the server closed the connection immediately.")
					fmt.Println("Check server logs or try connecting with curl to verify.")
				} else {
					fmt.Printf("\n[%s] Stream ended (EOF) - received %d snapshots, read %d lines\n",
						time.Now().Format("15:04:05.000"), snapshotCount, lineCount)
				}
				return
			}
			fmt.Fprintf(os.Stderr, "\n[ERROR] Stream read error: %v\n", err)
			return
		}

		lineCount++
		line = strings.TrimSpace(line)

		// Debug: log first 10 lines to see what we're getting
		if lineCount <= 10 {
			fmt.Printf("[DEBUG] Line %d (%d bytes): %q\n", lineCount, len(line), line)
		}

		// Skip empty lines
		if line == "" {
			continue
		}

		if strings.HasPrefix(line, ":") {
			// SSE comment/keepalive
			continue
		}

		// Handle SSE data lines
		if strings.HasPrefix(line, "data: ") {
			data := strings.TrimSpace(line[6:])
			if data == "" {
				continue
			}

			var snap Snapshot
			if err := json.Unmarshal([]byte(data), &snap); err == nil {
				snapshotCount++
				printSnapshot(snapshotCount, &snap)
			} else {
				fmt.Printf("[ERROR] Failed to parse snapshot #%d: %v\n", snapshotCount+1, err)
				fmt.Printf("Raw data (first 300 chars): %s\n",
					func() string {
						if len(data) > 300 {
							return data[:300] + "..."
						}
						return data
					}())
			}
		} else if strings.HasPrefix(line, "event:") || strings.HasPrefix(line, "id:") {
			// Skip SSE metadata lines
			if lineCount <= 10 {
				fmt.Printf("[DEBUG] SSE metadata: %s\n", line)
			}
			continue
		} else {
			// Log unexpected lines for debugging
			if lineCount <= 20 {
				fmt.Printf("[DEBUG] Unexpected line %d: %q\n", lineCount, line)
			}
		}
	}
}

func min(a, b int) int {
	if a < b {
		return a
	}
	return b
}

func printSnapshot(count int, snap *Snapshot) {
	timestamp := time.Now().Format("15:04:05.000")

	// Format VRAM sizes
	totalVRAMGB := float64(snap.TotalVRAMBytes) / (1024 * 1024 * 1024)
	allocatedVRAMGB := float64(snap.AllocatedVRAMBytes) / (1024 * 1024 * 1024)
	usedKVCacheGB := float64(snap.UsedKVCacheBytes) / (1024 * 1024 * 1024)

	// Calculate KV cache usage percentage
	var kvCacheUsagePerc float64
	if snap.TotalVRAMBytes > 0 {
		kvCacheUsagePerc = (float64(snap.UsedKVCacheBytes) / float64(snap.TotalVRAMBytes)) * 100.0
	}

	fmt.Printf("\n[%s] Snapshot #%d\n", timestamp, count)
	fmt.Println(strings.Repeat("─", 80))

	// Key metrics we care about
	fmt.Printf("📊 KEY METRICS:\n")
	fmt.Printf("   Prefix Cache Hit Rate: %.2f%%\n", snap.PrefixCacheHitRate)
	fmt.Printf("   KV Cache Usage:        %.2f%% (%.2f GB / %.2f GB total)\n",
		kvCacheUsagePerc, usedKVCacheGB, totalVRAMGB)

	fmt.Printf("\n💾 VRAM DETAILS:\n")
	fmt.Printf("   Total VRAM:     %.2f GB (%d bytes)\n", totalVRAMGB, snap.TotalVRAMBytes)
	fmt.Printf("   Allocated VRAM: %.2f GB (%d bytes)\n", allocatedVRAMGB, snap.AllocatedVRAMBytes)
	fmt.Printf("   Used KV Cache:  %.2f GB (%d bytes)\n", usedKVCacheGB, snap.UsedKVCacheBytes)

	if len(snap.Models) > 0 {
		fmt.Printf("\n🤖 MODELS (%d):\n", len(snap.Models))
		for i, model := range snap.Models {
			modelVRAMGB := float64(model.AllocatedVRAMBytes) / (1024 * 1024 * 1024)
			modelKVCacheGB := float64(model.UsedKVCacheBytes) / (1024 * 1024 * 1024)
			var modelKVPerc float64
			if model.AllocatedVRAMBytes > 0 {
				modelKVPerc = (float64(model.UsedKVCacheBytes) / float64(model.AllocatedVRAMBytes)) * 100.0
			}
			fmt.Printf("   [%d] %s (port %d)\n", i+1, model.ModelID, model.Port)
			fmt.Printf("        VRAM: %.2f GB | KV Cache: %.2f GB (%.1f%%)\n",
				modelVRAMGB, modelKVCacheGB, modelKVPerc)
		}
	} else {
		fmt.Println("\n⚠️  No models deployed")
	}

	fmt.Println(strings.Repeat("═", 80))
}
