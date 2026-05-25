import json
import os

log_path = "/Users/joshua/.gemini/antigravity/brain/0fcd7daa-48ba-4c23-8e7e-699765f078b6/.system_generated/logs/transcript.jsonl"
out_dir = "/Users/joshua/Developer/PedalForge/scratch/views"
os.makedirs(out_dir, exist_ok=True)

lines_to_extract = [11664, 11670, 11841, 11985, 12088, 12228, 12342, 12430, 12467]

with open(log_path, 'r', errors='ignore') as f:
    for idx, line in enumerate(f):
        line_num = idx + 1
        if line_num in lines_to_extract:
            print(f"Extracting line {line_num}...")
            try:
                obj = json.loads(line)
                step = obj.get('step_index')
                source = obj.get('source')
                t = obj.get('type')
                
                content = obj.get('content', '')
                out_path = os.path.join(out_dir, f"line_{line_num}_step_{step}_{source}_{t}.txt")
                with open(out_path, 'w', encoding='utf-8') as out_f:
                    out_f.write(content)
                print(f"  Saved to {out_path} (len={len(content)})")
                
                # Check if it has tool calls
                tc_list = obj.get('tool_calls', [])
                if tc_list:
                    print(f"  Has {len(tc_list)} tool calls")
                    for i, tc in enumerate(tc_list):
                        name = tc.get('name')
                        args = tc.get('args', {})
                        args_path = os.path.join(out_dir, f"line_{line_num}_step_{step}_tc_{i}_{name}.json")
                        with open(args_path, 'w', encoding='utf-8') as args_f:
                            json.dump(args, args_f, indent=2)
                        print(f"    Saved tool call args to {args_path}")
            except Exception as e:
                print(f"  Error on line {line_num}: {e}")

print("Extraction complete!")
