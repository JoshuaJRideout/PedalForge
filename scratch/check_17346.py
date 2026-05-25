import json
with open("/Users/joshua/Developer/PedalForge/scratch/step_17346.json", 'r') as f:
    text = f.read()
    print("LENGTH:", len(text))
    print("END OF TEXT:")
    print(text[-500:])
