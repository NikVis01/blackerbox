#!/usr/bin/env python3
"""
Super simple perpetual chat client for vLLM OpenAI-compatible endpoints.
Usage: python chat.py <url_or_ip:port>
Example: python chat.py http://localhost:8000
         python chat.py 192.168.1.100:8000
"""

import sys
import requests
import json

def get_model_name(url):
    """Get the actual model name from /v1/models endpoint."""
    try:
        models_url = f"{url}/v1/models"
        response = requests.get(models_url, timeout=5)
        response.raise_for_status()
        data = response.json()
        if "data" in data and len(data["data"]) > 0:
            return data["data"][0]["id"]
    except Exception as e:
        print(f"Warning: Could not get model name from /v1/models: {e}")
    return None

def main():
    if len(sys.argv) < 2:
        print("Usage: python chat.py <url_or_ip:port>")
        print("Example: python chat.py http://localhost:8000")
        print("         python chat.py 192.168.1.100:8000")
        sys.exit(1)
    
    url = sys.argv[1].strip()
    if not url.startswith(("http://", "https://")):
        url = f"http://{url}"
    
    url = url.rstrip('/')
    endpoint = f"{url}/v1/chat/completions"
    
    print(f"Connecting to: {endpoint}")
    
    # Get the actual model name
    model_name = get_model_name(url)
    if not model_name:
        print("Error: Could not determine model name. Make sure the vLLM server is running.")
        sys.exit(1)
    
    print(f"Using model: {model_name}")
    print("Type 'quit' or Ctrl+C to exit. Type 'clear' to clear conversation history.\n")
    
    messages = []
    max_history = 20  # Keep last 20 messages to avoid context limit
    
    while True:
        try:
            user_input = input("You: ").strip()
            
            if user_input.lower() in ["quit", "exit", "q"]:
                print("Goodbye!")
                break
            
            if user_input.lower() == "clear":
                messages = []
                print("Conversation history cleared.\n")
                continue
            
            if not user_input:
                continue
            
            messages.append({"role": "user", "content": user_input})
            
            # Trim history if too long (keep system message + recent messages)
            if len(messages) > max_history:
                # Keep first message if it's a system message, then keep recent ones
                system_msg = messages[0] if messages and messages[0].get("role") == "system" else None
                messages = messages[-max_history:]
                if system_msg and (not messages or messages[0].get("role") != "system"):
                    messages.insert(0, system_msg)
            
            payload = {
                "model": model_name,
                "messages": messages,
                "temperature": 0.7,
                "max_tokens": 512  # Limit output to avoid issues
            }
            
            print("Assistant: ", end="", flush=True)
            
            try:
                response = requests.post(endpoint, json=payload, timeout=120)
                response.raise_for_status()
                data = response.json()
                assistant_msg = data["choices"][0]["message"]["content"]
                print(assistant_msg)
                print()
                messages.append({"role": "assistant", "content": assistant_msg})
            except requests.exceptions.HTTPError as e:
                if e.response is not None:
                    try:
                        error_data = e.response.json()
                        error_msg = error_data.get("error", {}).get("message", str(e))
                        if "maximum context length" in error_msg or "reduce the length" in error_msg:
                            print(f"Error: Conversation too long. Clearing history and retrying...\n")
                            messages = [messages[-1]]  # Keep only the last user message
                            # Retry with just the current message
                            retry_payload = {
                                "model": model_name,
                                "messages": messages,
                                "temperature": 0.7,
                                "max_tokens": 512
                            }
                            retry_response = requests.post(endpoint, json=retry_payload, timeout=120)
                            retry_response.raise_for_status()
                            retry_data = retry_response.json()
                            assistant_msg = retry_data["choices"][0]["message"]["content"]
                            print("Assistant: " + assistant_msg)
                            print()
                            messages.append({"role": "assistant", "content": assistant_msg})
                        else:
                            print(f"Error: {error_msg}\n")
                    except:
                        print(f"Error: {e.response.status_code} - {e.response.text}\n")
                else:
                    print(f"Error: {e}\n")
            except requests.exceptions.RequestException as e:
                print(f"Error: {e}\n")
            except (KeyError, json.JSONDecodeError) as e:
                print(f"Error parsing response: {e}\n")
            
        except KeyboardInterrupt:
            print("\n\nGoodbye!")
            break
        except Exception as e:
            print(f"Error: {e}\n")

if __name__ == "__main__":
    main()

