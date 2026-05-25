import glob
import os

files = glob.glob("/Users/joshua/Developer/PedalForge/scratch/views/*.txt")

for f in sorted(files):
    base = os.path.basename(f)
    print(f"\n==========================================")
    print(f"FILE: {base} (size={os.path.getsize(f)})")
    print(f"==========================================")
    with open(f, 'r') as file_obj:
        text = file_obj.read()
        print(text[:400])
        print("...")
        print(text[-200:])
