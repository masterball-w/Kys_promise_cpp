import struct
import os

BASE_DIR = r"d:\program\misc\kys-promise-main\cpp_reborn\build\Debug"
KDEF_IDX = os.path.join(BASE_DIR, "resource", "Kdef.idx")
KDEF_GRP = os.path.join(BASE_DIR, "resource", "Kdef.grp")
TALK_IDX = os.path.join(BASE_DIR, "resource", "talk.idx")
TALK_GRP = os.path.join(BASE_DIR, "resource", "talk.grp")
ALLDEF_GRP = os.path.join(BASE_DIR, "save", "alldef.grp")

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

def load_talks():
    idx_data = read_file(TALK_IDX)
    grp_data = read_file(TALK_GRP)
    
    indices = []
    for i in range(0, len(idx_data), 4):
        indices.append(get_int32(idx_data, i))
        
    return indices, grp_data

def find_talk_id(indices, grp_data, search_str):
    # KYS uses Big5 or GBK. Let's try to search by decoding.
    # Or just search byte sequence if we know it.
    # But python strings are unicode.
    
    # Try GBK (common for Simplified Chinese mods) or Big5 (Original TW)
    encodings = ['gbk', 'big5', 'utf-8']
    
    found_ids = []
    
    for i in range(len(indices)):
        start = indices[i]
        end = indices[i+1] if i+1 < len(indices) else len(grp_data)
        raw_text = grp_data[start:end]
        
        # Remove null terminator if present
        if raw_text and raw_text[-1] == 0:
            raw_text = raw_text[:-1]
            
        decoded = None
        for enc in encodings:
            try:
                decoded = raw_text.decode(enc)
                break
            except:
                continue
        
        if decoded and search_str in decoded:
            print(f"Found '{search_str}' in Talk ID {i}: {decoded}")
            found_ids.append(i)
            
    return found_ids

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
    
    # Simple disassembler
    pc = 0
    while pc < length_words:
        op = get_int16(grp_data, data_start + pc * 2)
        print(f"[{pc:04d}] Op: {op}", end="")
        
        args = []
        skip = 0
        
        if op == 3: # ModEvent
            # ModEvent(SceneId, EventId, Condition, ...)
            # Args: 13 words?
            # S, E, Cond, Data1..10
            skip = 13
            print(" (ModEvent) ", end="")
            
        elif op == 1: # Dialogue
            skip = 3
            print(" (Dialogue) ", end="")
            
        elif op == 68: # NewTalk0
            skip = 7
            print(" (NewTalk0) ", end="")

        elif op == 25: # Pan
            skip = 2
            print(" (Pan) ", end="")
            
        elif op == 0: # Exit/End?
            print(" (Stop/End)")
            
        # Add more opcodes as needed
        
        # Print args
        if skip > 0:
            for k in range(1, skip + 1):
                if pc + k < length_words:
                    val = get_int16(grp_data, data_start + (pc + k) * 2)
                    args.append(val)
            print(f"Args: {args}")
            pc += skip
        else:
            print("")
            
        pc += 1

def main():
    try:
        print("Loading data...")
        talk_indices, talk_grp = load_talks()
        event_indices, event_grp = load_events()
        
        print("\nSearching for dialogues...")
        # Search for "知道了" (Kong Bala)
        ids1 = find_talk_id(talk_indices, talk_grp, "知道了")
        # Search for "孔堂主" (Jin Xiansheng)
        ids2 = find_talk_id(talk_indices, talk_grp, "孔堂主")
        
        # Now search for Events that use these Talk IDs
        # We need to scan ALL events for Opcode 1 (Dialogue) or 68 (NewTalk) with these IDs.
        # Opcode 1: [1, TalkID, ?, ?]
        # Opcode 68: [68, TalkID, ?, ?, ?, ?, ?, ?]
        
        target_talk_ids = set(ids1 + ids2)
        print(f"\nScanning events for Talk IDs: {target_talk_ids}")
        
        found_events = {}
        
        for i in range(len(event_indices)):
            eid = i + 1
            start = event_indices[i]
            end = event_indices[i+1] if i+1 < len(event_indices) else len(event_grp)
            
            pc = start
            while pc < end:
                op = get_int16(event_grp, pc)
                
                talk_id = -1
                if op == 1: # Dialogue
                    talk_id = get_int16(event_grp, pc + 2) # Arg 1 is TalkID
                elif op == 68: # NewTalk0
                    talk_id = get_int16(event_grp, pc + 2) # Arg 1 is TalkID
                    
                if talk_id in target_talk_ids:
                    print(f"Event {eid} uses Talk {talk_id}")
                    found_events[eid] = talk_id
                    
                pc += 2 # Step by 2 bytes? No, instructions are variable length.
                # This simple scan is risky because we might jump into args.
                # But Opcode 1 and 68 are common enough.
                # A safer way is to parse properly, but that requires full opcode table.
                # Let's just try to find the sequence of bytes.
                
        # Better scan: Look for exact byte sequence of the instruction?
        # Op 1: 01 00 [TalkID] 00 ...
        # Op 68: 44 00 [TalkID] 00 ...
        
        # Let's just dump the events 2235 and 2234 as user suggested, plus any found ones.
        events_to_dump = [2234, 2235]
        events_to_dump.extend(found_events.keys())
        events_to_dump = sorted(list(set(events_to_dump)))
        
        for eid in events_to_dump:
            dump_event(eid, event_indices, event_grp)

    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    main()
