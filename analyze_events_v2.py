import struct
import os

BASE_DIR = r"d:\program\misc\kys-promise-main\cpp_reborn\build\Debug"
KDEF_IDX = os.path.join(BASE_DIR, "resource", "Kdef.idx")
KDEF_GRP = os.path.join(BASE_DIR, "resource", "Kdef.grp")
TALK_IDX = os.path.join(BASE_DIR, "resource", "talk.idx")
TALK_GRP = os.path.join(BASE_DIR, "resource", "talk.grp")

def read_file(path):
    with open(path, 'rb') as f:
        return f.read()

def get_int16(data, offset):
    return struct.unpack_from('<h', data, offset)[0]

def get_int32(data, offset):
    return struct.unpack_from('<i', data, offset)[0]

def load_events():
    idx_data = read_file(KDEF_IDX)
    grp_data = read_file(KDEF_GRP)
    indices = []
    for i in range(0, len(idx_data), 4):
        indices.append(get_int32(idx_data, i))
    return indices, grp_data

def dump_event(event_id, indices, grp_data):
    if event_id <= 0 or event_id > len(indices):
        print(f"Event {event_id} out of range.")
        return

    offset = indices[event_id - 1]
    next_offset = indices[event_id] if event_id < len(indices) else len(grp_data)
    
    length_bytes = next_offset - offset
    length_words = length_bytes // 2
    
    print(f"\n--- Dumping Event {event_id} (Offset {offset}, Length {length_words} words) ---")
    
    data_start = offset
    pc = 0
    while pc < length_words:
        op = get_int16(grp_data, data_start + pc * 2)
        print(f"[{pc:04d}] Op: {op}", end="")
        
        skip = 0
        if op == 3: # ModEvent
            skip = 13
            print(" (ModEvent) Args: ", end="")
        elif op == 1: # Dialogue
            skip = 3
            print(" (Dialogue) Args: ", end="")
        elif op == 68: # NewTalk0
            skip = 7
            print(" (NewTalk0) Args: ", end="")
        elif op == 25: # Pan
            skip = 4
            print(" (Pan) Args: ", end="")
        elif op == 50: # PlaySound
            skip = 1
            print(" (PlaySound) Args: ", end="")
        elif op == 0: # Exit
            print(" (Stop/End)")
        
        # Generic args printing
        args = []
        if skip > 0:
            for k in range(1, skip + 1):
                if pc + k < length_words:
                    val = get_int16(grp_data, data_start + (pc + k) * 2)
                    args.append(val)
            print(f"{args}")
            pc += skip
        else:
            print("")
            
        pc += 1

def main():
    try:
        print("Loading data...")
        event_indices, event_grp = load_events()
        
        # Dump Event 101 based on user log
        dump_event(101, event_indices, event_grp)
        
        # Also dump 2234 again to compare ModEvent args
        dump_event(2234, event_indices, event_grp)

    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    main()
