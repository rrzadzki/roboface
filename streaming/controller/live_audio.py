from time import sleep

import numpy
import pyaudio

import serial_protocol as proto

FORMAT = pyaudio.paInt16
CHANNELS = 2
RATE = 44100
CHUNK = 1024
MAX_AMP = 2600


def main():
    ser = proto.connect()
    sleep(1)

    p = pyaudio.PyAudio()
    stream = p.open(format=FORMAT, channels=CHANNELS, rate=RATE, input=True, frames_per_buffer=CHUNK)

    ser.write(bytes([proto.PLAY]))

    while True:
        data = stream.read(CHUNK)
        arr = numpy.frombuffer(data, dtype=numpy.int16)
        amp = int(numpy.max(numpy.abs(arr)))
        amp = min(amp / MAX_AMP, 1.0)
        value = int(amp * proto.VALUE_MAX)
        ser.write(proto.encode_frame([(0, value)]))
        sleep(0.1)


if __name__ == '__main__':
    main()
