#!/bin/bash
# Undeploy (spindown) a model by model_id or container name

set -e

# Load common functions and .env
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/../common.sh"
load_env

SERVER_URL="${BLACKBOX_SERVER_URL:-http://localhost:6767}"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

print_success() {
    echo -e "${GREEN}✓${NC} $1"
}

print_error() {
    echo -e "${RED}✗${NC} $1"
}

print_info() {
    echo -e "${YELLOW}ℹ${NC} $1"
}

if [ -z "$1" ]; then
    print_error "Usage: $0 <model_id_or_container_name>"
    echo ""
    echo "Examples:"
    echo "  $0 Qwen/Qwen2.5-7B-Instruct"
    echo "  $0 vllm-Qwen-Qwen2-5-7B-Instruct"
    exit 1
fi

MODEL_ID="$1"

print_info "Spinning down: $MODEL_ID"

# Make the request
RESPONSE=$(curl -s -X POST "$SERVER_URL/spindown" \
    -H "Content-Type: application/json" \
    -d "{\"model_id\":\"$MODEL_ID\"}")

if [ $? -ne 0 ]; then
    print_error "Failed to connect to server at $SERVER_URL"
    exit 1
fi

# Parse response
SUCCESS=$(echo "$RESPONSE" | grep -o '"success":[^,}]*' | cut -d':' -f2 | tr -d ' ')

if [ "$SUCCESS" = "true" ]; then
    print_success "Model spindown successful!"
    echo "$RESPONSE" | python3 -m json.tool 2>/dev/null || echo "$RESPONSE"
else
    print_error "Spindown failed!"
    echo "$RESPONSE" | python3 -m json.tool 2>/dev/null || echo "$RESPONSE"
    exit 1
fi

