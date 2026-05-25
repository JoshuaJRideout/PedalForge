import json

log_path = "/Users/joshua/.gemini/antigravity/brain/0fcd7daa-48ba-4c23-8e7e-699765f078b6/.system_generated/logs/transcript.jsonl"

with open(log_path, 'r', errors='ignore') as f:
    for idx, line in enumerate(f):
        if 'step_index' in line and '"step_index":14563' in line:
            print("Found Step 14563, line", idx+1)
            try:
                obj = json.loads(line)
                tc_list = obj.get('tool_calls', [])
                print("Num tool calls:", len(tc_list))
                for i, tc in enumerate(tc_list):
                    args = tc.get('args', {})
                    code = args.get('CodeContent', '')
                    print(f"Tool {i}: name={tc.get('name')}, code len={len(code)}")
                    if 'truncated' in code:
                        print("WARNING: Code has '<truncated' text inside!")
            except Exception as e:
                print("Error:", e)
