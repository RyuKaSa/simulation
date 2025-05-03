import os
import json

def get_cpp_hpp_files(root="."):
    file_data = {}
    
    for dirpath, _, filenames in os.walk(root):
        for filename in filenames:
            if filename.endswith((".cpp", ".hpp")):
                filepath = os.path.join(dirpath, filename)
                try:
                    with open(filepath, "r", encoding="utf-8") as f:
                        file_data[filepath] = f.read()
                except Exception as e:
                    file_data[filepath] = f"Could not read file: {e}"
    
    return file_data

if __name__ == '__main__':
    output_filename = "cpp_hpp_contents.json"
    files_dict = get_cpp_hpp_files()
    
    with open(output_filename, "w", encoding="utf-8") as json_file:
        json.dump(files_dict, json_file, indent=4)

    print(f"File contents saved to {output_filename}")