import os

overview_path = "/Users/joshua/.gemini/antigravity/brain/0fcd7daa-48ba-4c23-8e7e-699765f078b6/.system_generated/logs/overview.txt"
if os.path.exists(overview_path):
    print("Overview file exists, size:", os.path.getsize(overview_path))
    with open(overview_path, 'r', errors='ignore') as f:
        content = f.read()
        print("Contains 'paintMatrixMixerXL':", 'paintMatrixMixerXL' in content)
        print("Contains 'updateDisplayForFilePath':", 'updateDisplayForFilePath' in content)
        
        # Let's count how many times they appear
        print("paintMatrixMixerXL count:", content.count('paintMatrixMixerXL'))
else:
    print("Overview file does not exist!")
