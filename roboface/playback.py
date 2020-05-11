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

res = 25
max = 0x7f

sleep(1)

# From a list of positions, do a timed walk through
positions = [
	0,
	10,
	30,
	70,
	50,
	40,
	10,
	50,
	20,
	40,
	30,
	60,
	10,
	50,
	20,
	40,
	30,
	60,
	10,
	0
]

eye_positions = [
	100,
	100,
	100,
	100,
	0,
	100,
	100,
	100,
	100,
	100,
	0,
	100,
	100,
	100,
	100,
	100,
	0,
	100,
	100,
	100,
]

blink_counter = 0

eyes_open = [
	0x01,0xFF,
	0x02,0xFF,
	0x03,0xFF,
	0x04,0xFF
]

eyes_closed = [
	0x01,0x80,
	0x02,0x80,
	0x03,0x80,
	0x04,0x80
]

for i in range(len(positions)-1):
	start = positions[i]
	end   = positions[i+1]
	step  = 1
	if(end < start): step = -1
		
	for v in range(start,end,step):
		a = int(((v) / 100) * max) | 0x80
		print("mouth {}".format(a))
		ser.write([0x00,a])
		blink_counter = blink_counter + 1
		if blink_counter == 100:
			blink_counter = 0
			ser.write(eyes_closed)
		
		if blink_counter > 5:
			ser.write(eyes_open)
		
		sleep(0.0005)
		
ser.write(eyes_open)