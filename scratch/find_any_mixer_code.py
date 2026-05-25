import json

log_path = "/Users/joshua/.gemini/antigravity/brain/0fcd7daa-48ba-4c23-8e7e-699765f078b6/.system_generated/logs/transcript.jsonl"

def search_text(query):
    print(f"\n--- Searching for: {query} ---")
    with open(log_path, 'r', errors='ignore') as f:
        for i, line in enumerate(f):
            if query in line:
                try:
                    obj = json.loads(line)
                    step = obj.get('step_index')
                    source = obj.get('source')
                    t = obj.get('type')
                    print(f"Match on line {i+1} | Step {step} | Source {source} | Type {t}")
                    
                    # Print thinking and content if they contain it
                    thinking = obj.get('thinking', '')
                    content = obj.get('content', '')
                    if query in thinking:
                        print("  Found in thinking! Length:", len(thinking))
                    if query in content:
                        print("  Found in content! Length:", len(content))
                    
                    # Check tool calls
                    for tc in obj.get('tool_calls', []):
                        args = tc.get('args', {})
                        for k, v in args.items():
                            if query in str(v):
                                print(f"  Found in tool call arg '{k}'! Length:", len(str(v)))
                except Exception as e:
                    print("  Failed to parse:", e)

search_text("paintMatrixMixerXL")
search_text("mouseDownMatrixMixerXL")
