import json
import os

log_path = "/Users/joshua/.gemini/antigravity/brain/0fcd7daa-48ba-4c23-8e7e-699765f078b6/.system_generated/logs/transcript.jsonl"
line_num = 12088

with open(log_path, 'r', errors='ignore') as f:
    for idx, line in enumerate(f):
        if idx + 1 == line_num:
            print("Line length:", len(line))
            try:
                obj = json.loads(line)
                print("Keys:", obj.keys())
                print("Type:", obj.get('type'))
                print("Status:", obj.get('status'))
                
                content = obj.get('content', '')
                print("Content length:", len(content))
                
                # Check if it has truncated marker
                if 'truncated' in content:
                    print("WARNING: Content has '<truncated' text inside!")
                
                out_path = "/Users/joshua/Developer/PedalForge/scratch/view_file_17798.txt"
                with open(out_path, 'w', encoding='utf-8') as out_f:
                    out_f.write(content)
                print("Saved content to:", out_path)
            except Exception as e:
                print("Failed to parse JSON:", e)
            break
