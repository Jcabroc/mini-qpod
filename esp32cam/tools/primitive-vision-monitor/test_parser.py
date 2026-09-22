from parser import parse_line

sample = parse_line("MOTION state=LEFT x=24 y=58 area=86 score=13 threshold=18 luminance=121 light=NORMAL")
assert sample and sample.state == "LEFT" and sample.x == 24 and sample.area == 86
assert sample.light == 121 and sample.light_state == "NORMAL"
assert parse_line("boot: ESP32") is None
assert parse_line("MOTION malformed") is None
print("parser simulation: OK")
