import os

def convert_file_to_utf8_sig(file_path):
    try:
        with open(file_path, 'rb') as f:
            raw = f.read()
        
        # 1. 检查是否已经是 UTF-8 with BOM
        if raw.startswith(b'\xef\xbb\xbf'):
            print(f"[SKIP] Already UTF-8 with BOM: {file_path}")
            return
        
        # 2. 尝试用 UTF-8 读取
        try:
            content = raw.decode('utf-8')
            encoding_detected = 'utf-8'
        except UnicodeDecodeError:
            # 3. 失败则尝试用 GBK/GB18030 读取
            try:
                content = raw.decode('gb18030')
                encoding_detected = 'gb18030'
            except UnicodeDecodeError:
                print(f"[ERROR] Unknown encoding, failed to decode: {file_path}")
                return
        
        # 4. 用 UTF-8 with BOM (utf-8-sig) 重新写入
        with open(file_path, 'w', encoding='utf-8-sig', newline='') as f:
            f.write(content)
        
        print(f"[CONVERT] Translated from {encoding_detected} to UTF-8 with BOM: {file_path}")

    except Exception as e:
        print(f"[ERROR] Failed to process {file_path}: {e}")

def walk_and_convert(dir_path):
    for root, dirs, files in os.walk(dir_path):
        for file in files:
            ext = os.path.splitext(file)[1].lower()
            if ext in ['.h', '.cpp', '.c']:
                file_path = os.path.join(root, file)
                convert_file_to_utf8_sig(file_path)

if __name__ == '__main__':
    src_dir = r"e:\__Code\__Work\hard2ser\hard2Ser_3_0\src"
    print(f"Starting conversion in {src_dir}...")
    walk_and_convert(src_dir)
    print("Conversion finished.")
