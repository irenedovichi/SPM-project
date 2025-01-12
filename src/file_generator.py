import os
import sys
import random
import string
import json

# Base directory where all files and directories will be created
BASE_DIR = "data"

def ensure_base_dir():
    """Ensure the base directory exists."""
    os.makedirs(BASE_DIR, exist_ok=True)


def create_file(filename, size_mb, file_type="txt", pattern=None):
    """Create a file with the specified name, size in MB, and file type. Optionally, specify a pattern for binary files."""
    ensure_base_dir()
    
    extensions = {"txt": ".txt", "bin": ".bin"}
    if file_type in extensions:
        if not filename.endswith(extensions[file_type]):
            filename += extensions[file_type]
    else:
        print(f"Unknown file type: {file_type}. File not created. Supported types: txt, bin.")
        return
    
    filepath = os.path.join(BASE_DIR, filename)
    size_bytes = size_mb * 1024 * 1024  # Convert MB to bytes

    if file_type == "txt":
        with open(filepath, 'w') as f:
            remaining_bytes = size_bytes
            while remaining_bytes > 0:
                chunk_size = min(1024, remaining_bytes)
                random_text = ''.join(random.choices(string.ascii_letters + string.digits + ' ', k=chunk_size))
                f.write(random_text)
                remaining_bytes -= len(random_text)
        print(f"Text file '{filepath}' of size {size_mb} MB created.")

    elif file_type == "bin":
        with open(filepath, 'wb') as f:
            if pattern == "random":
                # Generate random binary data
                f.write(os.urandom(size_bytes))
            elif pattern == "incremental":
                # Patterned data: Incremental bytes (00, 01, ..., FF) repeated 
                data = bytearray((i % 256 for i in range(size_bytes)))
                f.write(data)
            else:
                # Default: Fill with null bytes 
                f.write(b'\0' * size_bytes)
        print(f"Binary file '{filepath}' of size {size_mb} MB created with {pattern} pattern.")

    else:
        print(f"Unknown file type: {file_type}. File not created. Supported types: txt, bin.")


def create_directory(dirname, file_specs):
    """Create a directory with files of given sizes and types."""
    ensure_base_dir()
    dirpath = os.path.join(BASE_DIR, dirname)
    os.makedirs(dirpath, exist_ok=True)

    for i, (size_mb, file_type) in enumerate(file_specs):
        file_name = f'{size_mb}{file_type}.{file_type}'
        create_file(os.path.join(dirname, file_name), size_mb, file_type)

    print(f"Directory '{dirpath}' created with {len(file_specs)} files.")


def main():
    ensure_base_dir()

    if len(sys.argv) < 3:
        print("Usage:")
        print("  python file_generator.py file <filename> <size_in_mb> [file type: bin|txt] [pattern for bin files: random|incremental]")
        print("  python file_generator.py dir <dirname> <size1:type1> <size2:type2> ...")
        return

    action = sys.argv[1]
    if action == "file" and len(sys.argv) >= 4:
        filename = sys.argv[2]
        size_mb = int(sys.argv[3])
        file_type = sys.argv[4] if len(sys.argv) >= 5 else "txt"
        pattern = sys.argv[5] if len(sys.argv) >= 6 else None
        create_file(filename, size_mb, file_type, pattern)

    elif action == "dir" and len(sys.argv) >= 4:
        dirname = sys.argv[2]
        file_specs = []
        for spec in sys.argv[3:]:
            size, file_type = spec.split(":")
            file_specs.append((int(size), file_type))
        create_directory(dirname, file_specs)
    else:
        print("Invalid arguments. Please refer to the usage.")


if __name__ == "__main__":
    main()