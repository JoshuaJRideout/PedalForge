import json

path = "/Users/joshua/Developer/PedalForge/scratch/step_17527.json"
with open(path, 'r') as f:
    data = json.load(f)
    tc = data.get('tool_calls', [])[0]
    args = tc.get('args', {})
    chunks = args.get('ReplacementChunks')
    if isinstance(chunks, str):
        try:
            chunks_parsed = json.loads(chunks, strict=False)
            print("PARSED CHUNKS TYPE:", type(chunks_parsed))
            print("NUMBER OF CHUNKS:", len(chunks_parsed))
            for i, c in enumerate(chunks_parsed):
                print(f"  Chunk {i}: Start={c.get('StartLine')}, End={c.get('EndLine')}")
                print(f"    Target: {repr(c.get('TargetContent')[:120])}")
                print(f"    Replacement: {repr(c.get('ReplacementContent')[:120])}")
        except Exception as e:
            print("ERROR PARSING CHUNKS STRING:", e)
