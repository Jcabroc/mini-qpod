"""Read the USB diagnostic, validate JPEG framing and save one camera frame."""
import pathlib
import time
import serial

out = pathlib.Path(__file__).resolve().parent / 'results'
out.mkdir(exist_ok=True)
s = serial.Serial()
s.port = 'COM25'
s.baudrate = 115200
s.timeout = 15
s.dtr = False
s.rts = False
s.open()
try:
    s.rts = True
    time.sleep(.1)
    s.rts = False
    deadline = time.monotonic() + 60
    with (out / 'camera-boot.log').open('wb') as log:
        while time.monotonic() < deadline:
            line = s.readline()
            log.write(line)
            if line.startswith(b'JPEG_BEGIN '):
                size, width, height = map(int, line.split()[1:])
                payload = s.readline().strip()
                (out / 'frame-hex.txt').write_bytes(payload)
                end = s.readline().strip()
                data = bytes.fromhex(payload.decode('ascii'))
                if len(data) != size or end != b'JPEG_END':
                    raise RuntimeError('Incomplete serial frame')
                if not data.startswith(b'\xff\xd8') or not data.endswith(b'\xff\xd9'):
                    raise RuntimeError('Invalid JPEG markers')
                path = out / 'camera.jpg'
                path.write_bytes(data)
                print(f'CAPTURE_OK {width}x{height} {size} bytes {path}')
                break
        else:
            raise RuntimeError('No JPEG received; inspect camera-boot.log')
finally:
    s.close()
