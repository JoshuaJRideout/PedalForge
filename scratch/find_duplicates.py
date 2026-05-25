import os

def find_files(name, path):
    result = []
    for root, dirs, files in os.walk(path):
        if '.git' in dirs:
            dirs.remove('.git')
        if name in files:
            result.append(os.path.join(root, name))
    return result

print("Searching for duplicate copies of DynamicDisplayComponent.h...")
copies = find_files("DynamicDisplayComponent.h", "/Users/joshua/Developer")
for c in copies:
    print(c, os.path.getsize(c))
    
copies2 = find_files("CanvasOverlay.h", "/Users/joshua/Developer")
for c in copies2:
    print(c, os.path.getsize(c))
