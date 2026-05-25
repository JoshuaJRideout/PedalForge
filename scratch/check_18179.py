import json
path = "/Users/joshua/Developer/PedalForge/scratch/views/line_12467_step_18179_tc_0_write_to_file.json"
with open(path, 'r') as f:
    data = json.load(f)
    print("KEYS:", data.keys())
    print("TARGET FILE:", data.get('TargetFile'))
    print("CODE CONTENT LEN:", len(data.get('CodeContent', '')))
    print("IS CODE TRUNCATED:", 'truncated' in data.get('CodeContent', ''))
