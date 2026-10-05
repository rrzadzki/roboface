'''
Input feature keyframes from the CLI and send them to the roboface.
'''

import serial
ser = serial.Serial(
  'COM4',
  115200,
  serial.EIGHTBITS,
  serial.PARITY_NONE,
  serial.STOPBITS_ONE,
  timeout=0.5
)


FRAME_STOP	= 0xff
PLAY		= 0xfe
STOP		= 0xfd

VALUE_MASK  = 0x80 # OR this with a value

features = [
	'Mouth',
	'Left eyelid U',
	'Left eyelid L',
	'Right eyelid U',
	'Right eyelid L'
]

'''
For configuring frames from the CLI
'''

def new_frame():
	f = []
	for i in range(len(features)):
		f.append(None)
	return f
	
def print_frame(f):
	for i in range(len(features)):
		sel = ''
		if i == current_feature:
			sel = '*'
		print(f'{sel}{features[i]}: {f[i]}')

def finish_frame():
	global current_feature, current_frame
	frames.append(current_frame)
	current_feature = 0
	current_frame = new_frame()

'''
For sending frames over serial
'''

'''
Map an input value in a given range to an output value in a given domain.
The output value will always be clamped to the output range.
'''
def map_value(val, in_min, in_max, out_min, out_max):
	if val < in_min: return out_min
	if val > in_max: return out_max
	in_dif = in_max - in_min
	val_pc = (val - in_min) / in_dif
	out_df = out_max - out_min
	return out_min + out_df * val_pc

''' Get an array of bytes to describe a single frame. '''
def prep_frame(frame):
	barr = []
	for i in range(len(features)):
		# ea value is a 0-100%; map that onto the accepted range
		if frame[i] is not None:
			v = int(map_value(frame[i],0,100,0,0x7e)) | 0x80
			barr = barr + [i,v]
	barr.append(0xff) # stopcode
	return barr

''' Get an array of bytes for all frames in the buffer. '''
def prep_all_frames():
	barr = []
	for frame in frames:
		barr = barr + prep_frame(frame)
	return barr

''' Print an array of bytes as hex. '''
def print_byte_array(barr):
	print(barr)
	for b in barr:
		print('{:02x} '.format(b),end='')
	print()
	
frames = []
current_frame = new_frame()
current_feature = 0

print('''
Interactively build some frames and output them to serial or a file that can be
replayed through a good terminal (like RealTerm) to control the face.
''')

running = True
while running:
	print(f'{len(frames)} frames in buffer.\n')
	print_frame(current_frame)
	
	if current_feature < len(features) -1:
		print("(s)kip, (f)inish frame, e(x)it >> ", end='')
	else:
		print("(f)inish frame >>", end='')
		
	cmd = input()
	
	if cmd.startswith('s'):
		if current_frame == len(features) - 1:
			print('already on the last feature')
			continue
		
		current_feature = current_feature + 1
		continue
	elif cmd.startswith('f'):
		finish_frame()
		continue
	elif cmd.startswith('x'):
		exit()
	elif cmd.startswith('t'): # for "transmit" (add it to the prompt)
		barr = prep_all_frames()
		print_byte_array(barr)
		print("Sending bytes over serial...",end=None)
		ser.write(barr)
		print("done.")
		continue
	elif cmd.startswith('p'):
		print("Sending PLAY")
		ser.write(PLAY)
		continue
	else:
		try:
			i = int(cmd)
			if i >= 0 and i <= 100:
				# Next val
				current_frame[current_feature] = i
				current_feature = current_feature + 1
			else:
				print('positions should be 0-100')
				continue
		except ValueError:
			print("I don't know what to do with that.")
		
	