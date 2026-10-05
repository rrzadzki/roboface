import pyaudio
import numpy
import audioop
import serial
from time import sleep

FORMAT = pyaudio.paInt16
CHANNELS = 2
RATE = 44100
RECORD_SECONDS = 5
CHUNK = 1024

MAX_AMP = 2600

ser = serial.Serial(
  'COM4',
  115200,
  serial.EIGHTBITS,
  serial.PARITY_NONE,
  serial.STOPBITS_ONE,
  timeout=0.5
)
sleep(5)

#mic = soundcard.default_microphone()
p = pyaudio.PyAudio()
stream = p.open(format=FORMAT, channels=CHANNELS, rate=RATE, input=True, frames_per_buffer=CHUNK)

maximum = 0
samples = []

while True:
	# record 1/10 of a second
	data = stream.read(1024)
	amp = audioop.max(data,2)
	if amp > maximum:
		maximum = amp
		print("New max: {}".format(maximum))
	amp = amp / MAX_AMP
	amp = numpy.min([amp,1])
	b = int(amp * 0x7f) | 0x80
	#print("Sending byte {} for raw {}".format(b,amp))
	sleep(.01)
	ser.write([0x00, b])
	