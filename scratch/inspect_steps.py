import json
import glob
import os

steps = sorted(glob.glob("/Users/joshua/Developer/PedalForge/scratch/step_*.json"))

for s in steps:
    base = os.path.basename(s)
    print(f"\n==========================================")
    print(f"FILE: {base}")
    print(f"==========================================")
    with open(s, 'r') as f:
        data = json.load(f)
        tool_calls = data.get('tool_calls', [])
        for tc in tool_calls:
            name = tc.get('name')
            args = tc.get('args', {})
            desc = args.get('Description', '')
            target = args.get('TargetFile', '')
            print(f"Tool: {name} | Target: {target}")
            print(f"Description: {desc}")
            
            # If replacement chunk
            chunks = args.get('ReplacementChunks', [])
            if chunks:
                print(f"Found {len(chunks)} replacement chunks.")
                for i, chunk in enumerate(chunks):
                    print(f"  Chunk {i+1}: Start {chunk.get('StartLine')} | End {chunk.get('EndLine')}")
                    print(f"  Target: {repr(chunk.get('TargetContent')[:100])}...")
                    print(f"  Replacement: {repr(chunk.get('ReplacementContent')[:100])}...")
            else:
                # Standard replace_file_content args
                start = args.get('StartLine')
                end = args.get('EndLine')
                if start is not None:
                    print(f"  Start: {start} | End: {end}")
                    print(f"  Target: {repr(args.get('TargetContent')[:100])}...")
                    print(f"  Replacement: {repr(args.get('ReplacementContent')[:100])}...")
