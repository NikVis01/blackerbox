#!/usr/bin/env python3
import json
import sys
import argparse
import requests
import time
from typing import Optional, Dict, Any

def chat_completion(
    url: str,
    messages: list,
    model: str = "Qwen/Qwen2.5-7B-Instruct",
    stream: bool = False,
    temperature: float = 0.7,
    max_tokens: Optional[int] = None
) -> Optional[Dict[str, Any]]:
    """Send a chat completion request to vLLM server."""
    # Remove trailing /vram or other paths, ensure we have base URL
    base_url = url.rstrip('/').split('/vram')[0].split('/v1')[0]
    endpoint = f"{base_url}/v1/chat/completions"
    
    payload = {
        "model": model,
        "messages": messages,
        "temperature": temperature,
        "stream": stream
    }
    
    if max_tokens:
        payload["max_tokens"] = max_tokens
    
    try:
        if stream:
            response = requests.post(endpoint, json=payload, stream=True, timeout=60)
            response.raise_for_status()
            
            for line in response.iter_lines(decode_unicode=True):
                if line:
                    if line.startswith("data: "):
                        data_str = line[6:]
                        if data_str.strip() == "[DONE]":
                            break
                        try:
                            data = json.loads(data_str)
                            if "choices" in data and len(data["choices"]) > 0:
                                delta = data["choices"][0].get("delta", {})
                                if "content" in delta:
                                    print(delta["content"], end="", flush=True)
                        except json.JSONDecodeError:
                            continue
            print()  # Newline after streaming
            return None
        else:
            response = requests.post(endpoint, json=payload, timeout=60)
            response.raise_for_status()
            return response.json()
    except requests.exceptions.RequestException as e:
        print(f"Error: {e}", file=sys.stderr)
        return None

def main():
    parser = argparse.ArgumentParser(description='Chat with Qwen model via vLLM')
    parser.add_argument('--url', default='http://localhost:8000',
                       help='vLLM server base URL (default: http://localhost:8000). Will auto-append /v1/chat/completions')
    parser.add_argument('--model', default='Qwen/Qwen2.5-7B-Instruct',
                       help='Model name (default: Qwen/Qwen2.5-7B-Instruct)')
    parser.add_argument('--prompt', '-p', type=str,
                       help='Single prompt message')
    parser.add_argument('--message', '-m', action='append', nargs=2, metavar=('ROLE', 'CONTENT'),
                       help='Add a message (role content). Can be used multiple times.')
    parser.add_argument('--stream', '-s', action='store_true',
                       help='Stream the response')
    parser.add_argument('--temperature', '-t', type=float, default=0.7,
                       help='Temperature (default: 0.7)')
    parser.add_argument('--max-tokens', type=int,
                       help='Maximum tokens to generate')
    parser.add_argument('--interactive', '-i', action='store_true',
                       help='Interactive chat mode')
    parser.add_argument('--loop', '-l', action='store_true',
                       help='Loop mode: send "hello, testing inference" every 5 seconds')
    
    args = parser.parse_args()
    
    messages = []
    
    if args.loop:
        print("Loop mode: sending 'hello, testing inference' every 5 seconds (Ctrl+C to stop)")
        messages = [{"role": "user", "content": "Yo burr, lmk what I can cook up with inference for u bru"}]
        try:
            while True:
                print(f"\n[{time.strftime('%H:%M:%S')}] Sending request...")
                response = chat_completion(
                    args.url,
                    messages,
                    model=args.model,
                    stream=args.stream,
                    temperature=args.temperature,
                    max_tokens=args.max_tokens
                )
                if response and not args.stream:
                    if "choices" in response and len(response["choices"]) > 0:
                        print(f"Response: {response['choices'][0]['message']['content'][:100]}...")
                time.sleep(5)
        except KeyboardInterrupt:
            print("\nStopped.")
    elif args.interactive:
        print("Interactive chat mode (type 'quit' or 'exit' to end)")
        print("=" * 60)
        while True:
            try:
                user_input = input("\nYou: ").strip()
                if user_input.lower() in ['quit', 'exit', 'q']:
                    break
                if not user_input:
                    continue
                
                messages.append({"role": "user", "content": user_input})
                
                print("Assistant: ", end="", flush=True)
                
                if args.stream:
                    chat_completion(
                        args.url,
                        messages,
                        model=args.model,
                        stream=True,
                        temperature=args.temperature,
                        max_tokens=args.max_tokens
                    )
                    # For streaming, we don't get the full response to add to context
                    # In a real implementation, you'd accumulate the streamed content
                else:
                    response = chat_completion(
                        args.url,
                        messages,
                        model=args.model,
                        stream=False,
                        temperature=args.temperature,
                        max_tokens=args.max_tokens
                    )
                    if response and "choices" in response:
                        assistant_msg = response["choices"][0]["message"]["content"]
                        messages.append({"role": "assistant", "content": assistant_msg})
                        print(assistant_msg)
                
            except KeyboardInterrupt:
                print("\nExiting...")
                break
    else:
        if args.prompt:
            messages = [{"role": "user", "content": args.prompt}]
        elif args.message:
            for role, content in args.message:
                messages.append({"role": role, "content": content})
        else:
            # Default: read from stdin
            prompt = sys.stdin.read().strip()
            if prompt:
                messages = [{"role": "user", "content": prompt}]
            else:
                print("Error: No prompt provided. Use --prompt, --message, or pipe input.", file=sys.stderr)
                sys.exit(1)
        
        response = chat_completion(
            args.url,
            messages,
            model=args.model,
            stream=args.stream,
            temperature=args.temperature,
            max_tokens=args.max_tokens
        )
        
        if response:
            if args.stream:
                pass  # Already printed during streaming
            else:
                if "choices" in response and len(response["choices"]) > 0:
                    print(response["choices"][0]["message"]["content"])
                else:
                    print(json.dumps(response, indent=2))

if __name__ == '__main__':
    main()

