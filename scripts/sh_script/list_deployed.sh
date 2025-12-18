#!/bin/bash
# List all deployed models

set -e

# Load common functions and .env
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/../common.sh"
load_env

SERVER_URL="${BLACKBOX_SERVER_URL:-http://localhost:6767}"

# Colors
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${BLUE}Deployed Models${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""

# Get models list from server
RESPONSE=$(curl -s --max-time 10 --connect-timeout 5 "$SERVER_URL/models" 2>&1)
CURL_EXIT=$?

if [ $CURL_EXIT -ne 0 ]; then
    echo "Error: Failed to connect to server at $SERVER_URL"
    if [ -n "$RESPONSE" ]; then
        echo "$RESPONSE"
    fi
    exit 1
fi

if [ -z "$RESPONSE" ]; then
    echo "Warning: Empty response from server"
    exit 1
fi

# Pretty print JSON if available
if command -v python3 &> /dev/null; then
    FORMATTED=$(echo "$RESPONSE" | python3 -m json.tool 2>/dev/null)
    if [ $? -eq 0 ] && [ -n "$FORMATTED" ]; then
        echo "$FORMATTED"
    else
        echo "$RESPONSE"
    fi
else
    echo "$RESPONSE"
fi

# Check if models array is empty
if echo "$RESPONSE" | grep -q '"models":\s*\[\]' || echo "$RESPONSE" | grep -q '"total":\s*0'; then
    echo ""
    echo "No models currently deployed."
fi

echo ""
echo "Docker containers:"
docker ps -a --filter name=vllm- --format "table {{.Names}}\t{{.Status}}\t{{.Ports}}" 2>/dev/null || echo "Docker command failed or no containers found"

