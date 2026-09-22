import struct
import sys

def extract_rich_header(pe_path, output_path):
    with open(pe_path, 'rb') as f:
        data = bytearray(f.read())
    
    rich_pos = data.find(b'Rich')
    if rich_pos == -1:
        print("No Rich Header found in source binary")
        return
    
    e_lfanew = struct.unpack_from('<I', data, 0x3C)[0]
    
    # Rich Header sits between 0x40 and e_lfanew
    rich_header = bytes(data[0x40:e_lfanew])
    
    with open(output_path, 'wb') as f:
        f.write(rich_header)
    
    print(f"[+] Extracted {len(rich_header)} bytes")
    print(f"[+] Saved to: {output_path}")

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print("Usage: extract_rich_header.py source.exe output.bin")
        sys.exit(1)
    extract_rich_header(sys.argv[1], sys.argv[2])