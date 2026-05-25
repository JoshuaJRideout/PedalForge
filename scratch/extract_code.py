import json
import os

log_path = "/Users/joshua/.gemini/antigravity/brain/0fcd7daa-48ba-4c23-8e7e-699765f078b6/.system_generated/logs/transcript.jsonl"

print(f"Reading logs from {log_path}...")
if not os.path.exists(log_path):
    print("Log file not found!")
    exit(1)

with open(log_path, 'r', encoding='utf-8') as f:
    for i, line in enumerate(f):
        if 'DynamicDisplayComponent.h' in line or 'CanvasOverlay.h' in line:
            try:
                data = json.loads(line)
                step = data.get('step_index')
                t = data.get('type')
                status = data.get('status')
                
                # Check tool calls
                tool_calls = data.get('tool_calls', [])
                for tc in tool_calls:
                    name = tc.get('name')
                    args = tc.get('args', {})
                    target = args.get('TargetFile', '') or args.get('Target') or ''
                    if 'DynamicDisplayComponent.h' in str(target) or 'CanvasOverlay.h' in str(target):
                        print(f"Line {i+1} | Step {step} | Tool {name} | Target {target}")
            except Exception as e:
                pass
