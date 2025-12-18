#!/bin/bash
# Deploy a single HuggingFace model to the blackbox server

set -e

# Load common functions and .env
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/../common.sh"
load_env

# Default values (can be overridden by .env or environment)
SERVER_URL="${BLACKBOX_SERVER_URL:-http://localhost:6767}"
HF_TOKEN="${HF_TOKEN:-}"
PORT="${PORT:-8000}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to print colored output
print_success() {
    echo -e "${GREEN}✓${NC} $1"
}

print_error() {
    echo -e "${RED}✗${NC} $1"
}

print_info() {
    echo -e "${YELLOW}ℹ${NC} $1"
}

# Check if model_id is provided
if [ -z "$1" ]; then
    print_error "Usage: $0 <model_id> [port] [hf_token]"
    echo ""
    echo "Examples:"
    echo "  $0 Qwen/Qwen2.5-7B-Instruct"
    echo "  $0 Qwen/Qwen2.5-7B-Instruct 8001"
    echo "  $0 Qwen/Qwen2.5-7B-Instruct 8001 hf_xxxxxxxxxxxxx"
    echo ""
    echo "Configuration:"
    echo "  Loads from .env file in project root or ~/.env"
    echo "  Environment variables (can override .env):"
    echo "    BLACKBOX_SERVER_URL - Server URL (default: http://localhost:6767)"
    echo "    HF_TOKEN - HuggingFace token (can also be passed as argument)"
    echo "    PORT - Port number (default: 8000, can also be passed as argument)"
    exit 1
fi

MODEL_ID="$1"
PORT="${2:-$PORT}"
HF_TOKEN="${3:-$HF_TOKEN}"

# Build JSON payload
JSON_PAYLOAD="{"
JSON_PAYLOAD+="\"model_id\":\"$MODEL_ID\""

if [ -n "$PORT" ]; then
    JSON_PAYLOAD+=",\"port\":$PORT"
fi

if [ -n "$HF_TOKEN" ]; then
    JSON_PAYLOAD+=",\"hf_token\":\"$HF_TOKEN\""
fi

JSON_PAYLOAD+="}"

print_info "Deploying model: $MODEL_ID"
print_info "Server: $SERVER_URL"
print_info "Port: $PORT"

# Make the request with status code tracking
# Set a long timeout (5 minutes) since large models can take time to deploy
HTTP_CODE=$(curl -s --max-time 300 -o /tmp/deploy_response.json -w "%{http_code}" -X POST "$SERVER_URL/deploy" \
    -H "Content-Type: application/json" \
    -d "$JSON_PAYLOAD")

RESPONSE=$(cat /tmp/deploy_response.json)
rm -f /tmp/deploy_response.json

# Check if curl succeeded
if [ $? -ne 0 ]; then
    print_error "Failed to connect to server at $SERVER_URL"
    exit 1
fi

# Check HTTP status code
if [ "$HTTP_CODE" != "200" ]; then
    print_error "Deployment failed with HTTP status: $HTTP_CODE"
    echo "$RESPONSE" | python3 -m json.tool 2>/dev/null || echo "$RESPONSE"
    exit 1
fi

# Parse response
SUCCESS=$(echo "$RESPONSE" | grep -o '"success":[^,}]*' | cut -d':' -f2 | tr -d ' ')

if [ "$SUCCESS" = "true" ]; then
    print_success "Model deployed successfully! (HTTP $HTTP_CODE OK)"
    echo "$RESPONSE" | python3 -m json.tool 2>/dev/null || echo "$RESPONSE"
    
    CONTAINER_ID=$(echo "$RESPONSE" | grep -o '"container_id":"[^"]*"' | cut -d'"' -f4)
    if [ -n "$CONTAINER_ID" ]; then
        print_info "Container ID: $CONTAINER_ID"
        print_info "Check status: docker ps | grep $CONTAINER_ID"
    fi
else
    print_error "Deployment failed! (HTTP $HTTP_CODE)"
    echo "$RESPONSE" | python3 -m json.tool 2>/dev/null || echo "$RESPONSE"
    exit 1
fi

