import hashlib
from pathlib import Path
import esptool

p = Path('backups/original-4MB.bin')
data = bytearray(p.read_bytes())
assert len(data) == 0x400000
esp = esptool.detect_chip('COM25', 115200)
try:
    esp = esp.run_stub()
    for offset in range(0, len(data), 0x10000):
        local = hashlib.md5(data[offset:offset+0x10000]).hexdigest()
        remote = esp.flash_md5sum(offset, 0x10000)
        if local != remote:
            print(f'Rereading changed block 0x{offset:x}', flush=True)
            data[offset:offset+0x10000] = esp.read_flash(offset, 0x10000)
    assert hashlib.md5(data).hexdigest() == esp.flash_md5sum(0, len(data)), 'Backup mismatch'
    p.write_bytes(data)
    Path('backups/original-4MB.sha256').write_text(hashlib.sha256(data).hexdigest()+'\n')
    print('BACKUP_VERIFIED 4194304 bytes', flush=True)
finally:
    esp._port.close()
