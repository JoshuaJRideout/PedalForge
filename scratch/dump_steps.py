import json
import os

log_path = "/Users/joshua/.gemini/antigravity/brain/0fcd7daa-48ba-4c23-8e7e-699765f078b6/.system_generated/logs/transcript.jsonl"

steps_to_extract = [14883, 14889, 17326, 17330, 17334, 17346, 17527, 17542, 17789, 17799, 17803]

with open(log_path, 'r', errors='ignore') as f:
    for line in f:
        try:
            obj = json.loads(line)
            step = obj.get('step_index')
            if step in steps_to_extract:
                print(f"--- DUMPING STEP {step} ---")
                out_path = f"/Users/joshua/Developer/PedalForge/scratch/step_{step}.json"
                with open(out_path, 'w') as out_f:
                    json.dump(obj, out_f, indent=2)
                print(f"Saved to {out_path}")
        except:
            pass
