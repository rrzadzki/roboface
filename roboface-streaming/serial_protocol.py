import os

import serial

FRAME_START = 0xff
PLAY = 0xfe
STOP = 0xfd
STOP_DL = 0xfc
START_DL = 0xfb

VALUE_MAX = 0xff


def port_name():
    return os.environ.get('ROBOFACE_PORT', 'COM3')


def connect():
    return serial.Serial(
        port_name(),
        115200,
        serial.EIGHTBITS,
        serial.PARITY_NONE,
        serial.STOPBITS_ONE,
        timeout=0.5,
    )


def pct_to_value(pct):
    return int(round(pct / 100.0 * VALUE_MAX))


def encode_frame(pairs):
    out = bytearray([FRAME_START, len(pairs)])
    for channel, value in pairs:
        out.append(channel & 0xff)
        out.append(value & 0xff)
    return bytes(out)
