import json

log_path = "/Users/joshua/.gemini/antigravity/brain/0fcd7daa-48ba-4c23-8e7e-699765f078b6/.system_generated/logs/transcript.jsonl"

line_num = 11494 # 1-indexed

with open(log_path, 'r', errors='ignore') as f:
    for idx, line in enumerate(f):
        if idx + 1 == line_num:
            print("Line length:", len(line))
            try:
                obj = json.loads(line)
                print("Keys:", obj.keys())
                print("Step Index:", obj.get('step_index'))
                print("Source:", obj.get('source'))
                print("Type:", obj.get('type'))
                print("Thinking length:", len(obj.get('thinking', '')))
                tool_calls = obj.get('tool_calls', [])
                print("Number of tool calls:", len(tool_calls))
                if tool_calls:
                    print("Tool call name:", tool_calls[0].get('name'))
                    args = tool_calls[0].get('args', {})
                    print("Args keys:", args.keys())
                    print("ReplacementContent length:", len(args.get('ReplacementContent', '')))
            except Exception as e:
                print("Failed to parse JSON:", e)
            break
