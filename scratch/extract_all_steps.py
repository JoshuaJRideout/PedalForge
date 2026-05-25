import json
import os

log_path = "/Users/joshua/.gemini/antigravity/brain/0fcd7daa-48ba-4c23-8e7e-699765f078b6/.system_generated/logs/transcript.jsonl"
out_dir = "/Users/joshua/Developer/PedalForge/scratch/extracted"
os.makedirs(out_dir, exist_ok=True)

steps = [14883, 14889, 17326, 17330, 17334, 17346, 17527, 17542, 17789, 17799, 17803]

with open(log_path, 'r', errors='ignore') as f:
    for idx, line in enumerate(f):
        try:
            obj = json.loads(line)
            step = obj.get('step_index')
            if step in steps:
                print(f"Extracting step {step}...")
                tc_list = obj.get('tool_calls', [])
                for i, tc in enumerate(tc_list):
                    name = tc.get('name')
                    args = tc.get('args', {})
                    
                    # Save the arguments directly as pretty JSON
                    out_path = os.path.join(out_dir, f"step_{step}_tc_{i}_{name}.json")
                    with open(out_path, 'w', encoding='utf-8') as out_f:
                        json.dump(args, out_f, indent=2, ensure_ascii=False)
                    
                    # If it's a replacement content, save that separately as C++
                    if 'ReplacementContent' in args:
                        cpp_path = os.path.join(out_dir, f"step_{step}_tc_{i}_content.cpp")
                        with open(cpp_path, 'w', encoding='utf-8') as cpp_f:
                            cpp_f.write(args['ReplacementContent'])
                        
                        target_path = os.path.join(out_dir, f"step_{step}_tc_{i}_target.cpp")
                        with open(target_path, 'w', encoding='utf-8') as target_f:
                            target_f.write(args['TargetContent'])
                            
                    # If it's multi_replace_file_content, parse chunks and save them
                    if 'ReplacementChunks' in args:
                        chunks = args['ReplacementChunks']
                        if isinstance(chunks, str):
                            try:
                                chunks = json.loads(chunks, strict=False)
                            except Exception as e:
                                print(f"Error parsing chunks string in step {step}: {e}")
                        
                        if isinstance(chunks, list):
                            for j, chunk in enumerate(chunks):
                                chunk_path = os.path.join(out_dir, f"step_{step}_tc_{i}_chunk_{j}.json")
                                with open(chunk_path, 'w', encoding='utf-8') as ch_f:
                                    json.dump(chunk, ch_f, indent=2, ensure_ascii=False)
                                
                                # Also write target and replacement separately
                                with open(os.path.join(out_dir, f"step_{step}_tc_{i}_chunk_{j}_target.cpp"), 'w', encoding='utf-8') as f_t:
                                    f_t.write(chunk.get('TargetContent', ''))
                                with open(os.path.join(out_dir, f"step_{step}_tc_{i}_chunk_{j}_replacement.cpp"), 'w', encoding='utf-8') as f_r:
                                    f_r.write(chunk.get('ReplacementContent', ''))
        except Exception as e:
            pass

print("Extraction complete!")
