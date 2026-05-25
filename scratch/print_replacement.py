import json

log_path = "/Users/joshua/.gemini/antigravity/brain/0fcd7daa-48ba-4c23-8e7e-699765f078b6/.system_generated/logs/transcript.jsonl"
line_num = 11494

with open(log_path, 'r', errors='ignore') as f:
    for idx, line in enumerate(f):
        if idx + 1 == line_num:
            obj = json.loads(line)
            tc = obj.get('tool_calls', [])[0]
            args = tc.get('args', {})
            print("REPLACEMENT CONTENT:")
            print(args.get('ReplacementContent'))
            break
