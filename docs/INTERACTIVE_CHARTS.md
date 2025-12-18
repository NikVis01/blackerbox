# Implementing Interactive Charts with Mouse Hover

## Overview

To add granular, interactive charts with "hover to see exact values" functionality to the existing Bubble Tea dashboard, you need to:

1. **Enable mouse tracking** - Add `tea.WithMouseAllMotion()` to program initialization
2. **Handle mouse messages** - Process `tea.MouseMsg` in the Update function
3. **Map coordinates to data** - Convert mouse X position to data point index
4. **Display tooltips** - Show exact values at hover position
5. **Optional: Use ntcharts** - For higher resolution (Braille-based) charts

## Step-by-Step Implementation

### 1. Enable Mouse Tracking

**File: `blackbox-cli/cmd/root.go`**

Change:
```go
p := tea.NewProgram(m, tea.WithAltScreen())
```

To:
```go
p := tea.NewProgram(m, tea.WithAltScreen(), tea.WithMouseAllMotion())
```

**Why:** `WithMouseAllMotion()` enables continuous mouse position updates, not just clicks. Without this, you only get click events.

### 2. Add Mouse State to Model

**File: `blackbox-cli/internal/ui/dashboard.go`**

Add to `DashboardModel` struct:
```go
type DashboardModel struct {
    // ... existing fields ...
    
    // Mouse tracking for interactive charts
    mouseX            int      // Current mouse X position
    mouseY            int      // Current mouse Y position
    hoveredChartIndex int      // Which chart is being hovered (0=VRAM, 1=KV Cache, 2=Prefix Hit Rate)
    hoveredDataIndex  int      // Which data point in history is hovered
    showTooltip       bool     // Whether to show tooltip
}
```

### 3. Handle Mouse Messages in Update

**File: `blackbox-cli/internal/ui/dashboard.go`**

Add to the `Update` function's switch statement:
```go
case tea.MouseMsg:
    if msg.Action == tea.MouseActionMotion {
        m.mouseX = msg.X
        m.mouseY = msg.Y
        
        // Determine which chart is being hovered based on Y position
        // Charts are rendered at specific Y positions in the view
        chartStartY := 10 // Adjust based on your layout
        chartHeight := 6  // Height of each chart
        
        if m.mouseY >= chartStartY && m.mouseY < chartStartY+chartHeight {
            m.hoveredChartIndex = 0 // VRAM chart
        } else if m.mouseY >= chartStartY+chartHeight && m.mouseY < chartStartY+chartHeight*2 {
            m.hoveredChartIndex = 1 // KV Cache chart
        } else if m.mouseY >= chartStartY+chartHeight*2 && m.mouseY < chartStartY+chartHeight*3 {
            m.hoveredChartIndex = 2 // Prefix Hit Rate chart
        } else {
            m.showTooltip = false
            return m, nil
        }
        
        // Map mouse X to data point index
        chartWidth := m.width - 20 // Adjust based on your chart width
        if chartWidth > 0 && len(m.history) > 0 {
            // Calculate which data point corresponds to mouse X
            // Charts show last N points, so we need to map X to history index
            displayCount := min(len(m.history), chartWidth-2)
            if displayCount > 0 {
                // X=0 is left edge, X=chartWidth is right edge
                // Right edge = most recent data
                relativeX := m.mouseX - 1 // Account for Y-axis
                if relativeX >= 0 && relativeX < chartWidth-2 {
                    // Map to history index (most recent is last in array)
                    startIdx := len(m.history) - displayCount
                    dataIdx := startIdx + (relativeX * displayCount / (chartWidth - 2))
                    if dataIdx >= 0 && dataIdx < len(m.history) {
                        m.hoveredDataIndex = dataIdx
                        m.showTooltip = true
                    }
                }
            }
        }
    }
    return m, nil
```

### 4. Create Tooltip Rendering Function

**File: `blackbox-cli/internal/ui/dashboard.go`**

Add function to render tooltip:
```go
func (m *DashboardModel) renderTooltip() string {
    if !m.showTooltip || m.hoveredDataIndex < 0 || m.hoveredDataIndex >= len(m.history) {
        return ""
    }
    
    dp := m.history[m.hoveredDataIndex]
    var value string
    var label string
    
    switch m.hoveredChartIndex {
    case 0: // VRAM Usage
        gb := float64(dp.AllocatedVRAMBytes) / (1024 * 1024 * 1024)
        value = fmt.Sprintf("%.2f GB", gb)
        label = "Allocated VRAM"
    case 1: // KV Cache
        gb := float64(dp.UsedKVCacheBytes) / (1024 * 1024 * 1024)
        value = fmt.Sprintf("%.2f GB", gb)
        label = "Used KV Cache"
    case 2: // Prefix Hit Rate
        value = fmt.Sprintf("%.2f%%", dp.PrefixCacheHitRate)
        label = "Prefix Cache Hit Rate"
    default:
        return ""
    }
    
    timeStr := dp.Time.Format("15:04:05")
    tooltip := fmt.Sprintf("%s: %s @ %s", label, value, timeStr)
    
    // Position tooltip near mouse, but ensure it fits on screen
    tooltipX := m.mouseX
    if tooltipX+len(tooltip) > m.width {
        tooltipX = m.width - len(tooltip) - 1
    }
    if tooltipX < 0 {
        tooltipX = 0
    }
    
    tooltipY := m.mouseY
    if tooltipY+1 > m.height {
        tooltipY = m.height - 2
    }
    
    // Use lipgloss to style and position tooltip
    style := lipgloss.NewStyle().
        Foreground(lipgloss.Color("15")).
        Background(lipgloss.Color("236")).
        Padding(0, 1).
        Border(lipgloss.RoundedBorder()).
        BorderForeground(lipgloss.Color("39"))
    
    return lipgloss.Position(tooltipX, tooltipY, style.Render(tooltip))
}
```

### 5. Integrate Tooltip into View

**File: `blackbox-cli/internal/ui/render.go`**

In your main render function, add tooltip at the end:
```go
func (m *DashboardModel) View() string {
    // ... existing rendering code ...
    
    // Add tooltip overlay
    tooltip := m.renderTooltip()
    if tooltip != "" {
        // Render tooltip on top of everything
        return lipgloss.JoinVertical(lipgloss.Left, mainContent, tooltip)
    }
    
    return mainContent
}
```

### 6. Coordinate Mapping Formula

The key formula for mapping mouse X to data index:

```
relativeX = mouseX - chartLeftEdge
dataIndex = startIndex + (relativeX * displayCount / chartWidth)
```

Where:
- `startIndex = len(history) - displayCount` (first visible data point)
- `displayCount = min(len(history), chartWidth - 2)` (number of points shown)
- `chartWidth = actual chart width in characters`

### 7. Optional: Using ntcharts for Higher Resolution

If you want 4x resolution using Braille characters:

**Add to `go.mod`:**
```bash
go get github.com/NimbleMarkets/ntcharts
```

**Example integration:**
```go
import (
    "github.com/NimbleMarkets/ntcharts/linechart/timeserieslinechart"
)

// In DashboardModel:
chart *timeserieslinechart.Model

// Initialize in NewDashboard:
chart := timeserieslinechart.New(
    timeserieslinechart.WithUpdateInterval(time.Millisecond * 100),
)

// Update chart with new data point:
chart.AddDataPoint(time.Now(), float64(snapshot.AllocatedVRAMBytes))

// In View, render chart:
chartView := chart.View()
```

## Key Points

1. **Mouse tracking requires `WithMouseAllMotion()`** - Without it, you only get clicks
2. **Coordinate mapping is critical** - Map terminal X/Y to data indices accurately
3. **Tooltip positioning** - Ensure tooltips don't go off-screen
4. **Performance** - Mouse events fire frequently; keep tooltip calculations lightweight
5. **Chart boundaries** - Only show tooltips when mouse is actually over a chart

## Testing

1. Enable mouse tracking in program initialization
2. Move mouse over charts - tooltip should appear
3. Verify exact values match the data point at that X position
4. Check tooltip doesn't overflow screen edges

