import serial
from time import sleep

ser = serial.Serial(
  'COM4',
  115200,
  serial.EIGHTBITS,
  serial.PARITY_NONE,
  serial.STOPBITS_ONE,
  timeout=0.5
)
sleep(5)

# look for bytes, print'em, wait a second, do it again.
# until they're all gone.

  
test_values = bytearray([
  42 & (1 << 4),
  42 & (2 << 4),
  41 & (3 << 4),
  43 & (4 << 4)
])

for b in test_values:
  print("bytes available: ")
  print(ser.in_waiting);
  print("writing byte: ")
  print("{0:b}".format(b))
  ser.write(b)
  sleep(1)
  print("=result=")
  while ser.in_waiting > 0:
    print(ser.read_until())
#print(ser.read(1000))