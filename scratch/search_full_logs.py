import json

log_path = "/Users/joshua/.gemini/antigravity/brain/0fcd7daa-48ba-4c23-8e7e-699765f078b6/.system_generated/logs/transcript.jsonl"

with open(log_path, 'r', errors='ignore') as f:
    for i, line in enumerate(f):
        if 'paintMatrixMixerXL' in line:
            try:
                obj = json.loads(line)
                step = obj.get('step_index')
                source = obj.get('source')
                t = obj.get('type')
                print(f"Match on line {i+1} | Step {step} | Source {source} | Type {t}")
            except Exception as e:
                print(f"Match on line {i+1} but failed to parse JSON: {e}")
