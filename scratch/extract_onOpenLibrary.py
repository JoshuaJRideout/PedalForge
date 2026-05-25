import json

log_path = "/Users/joshua/.gemini/antigravity/brain/0fcd7daa-48ba-4c23-8e7e-699765f078b6/.system_generated/logs/transcript.jsonl"

with open(log_path, 'r', errors='ignore') as f:
    for idx, line in enumerate(f):
        if idx + 1 == 9227 or idx + 1 == 9228:
            print(f"=== Line {idx+1} ===")
            try:
                obj = json.loads(line)
                print("Keys:", obj.keys())
                print("Type:", obj.get('type'))
                print("Thinking:", obj.get('thinking', '')[:300])
                tool_calls = obj.get('tool_calls', [])
                if tool_calls:
                    print("Tool Name:", tool_calls[0].get('name'))
                    args = tool_calls[0].get('args', {})
                    print("Target:", args.get('TargetFile'))
                    print("Instruction:", args.get('Instruction'))
                    print("ReplacementContent:")
                    print(args.get('ReplacementContent'))
                else:
                    print("Content:", obj.get('content', '')[:500])
            except Exception as e:
                print("Error:", e)
