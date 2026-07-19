import struct

basePath = 'd:/Program/github/Kys_promise_cpp-master/cpp_reborn/tests/resource/'

with open(basePath + 'talk.idx', 'rb') as f:
    idxData = f.read()

count = len(idxData) // 4
print(f'Total dialogues: {count}')

indices = struct.unpack(f'<{count}i', idxData)

with open(basePath + 'talk.grp', 'rb') as f:
    grpData = f.read()

talkId = 247
actualTalkNum = talkId - 1

offset = indices[actualTalkNum]
nextOffset = indices[actualTalkNum + 1] if actualTalkNum + 1 < count else len(grpData)
length = nextOffset - offset

print(f'Talk ID: {talkId}')
print(f'Offset: {offset}')
print(f'Length: {length}')
print()

rawBytes = grpData[offset:offset+min(length, 200)]
print('Raw bytes (XOR 0xFF encoded):')
print(' '.join(f'{b:02X}' for b in rawBytes[:100]))
print()

decoded = bytes([b ^ 0xFF for b in rawBytes])
print('Decoded bytes (after XOR 0xFF):')
print(' '.join(f'{b:02X}' for b in decoded[:100]))
print()

print('Decoded string (as GBK):')
try:
    text = decoded.decode('gbk', errors='replace')
    print(repr(text[:200]))
except Exception as e:
    print(f'Decode error: {e}')

print()
print('Looking for specific characters:')
print('头 in GBK: CD B7')
print('疼 in GBK: CC DB')
print('过 in GBK: B9 FD')

print()
print('Searching in decoded bytes:')
for i in range(len(decoded) - 1):
    if (decoded[i] == 0xCD and decoded[i+1] == 0xB7) or \
       (decoded[i] == 0xCC and decoded[i+1] == 0xDB) or \
       (decoded[i] == 0xB9 and decoded[i+1] == 0xFD):
        print(f'Found at position {i}: {decoded[i]:02X} {decoded[i+1]:02X}')
