from time import sleep

import serial_protocol as proto

MOUTH = 0
EYES = [1, 2, 3, 4]

MOUTH_POSITIONS = [0, 10, 30, 70, 50, 40, 10, 50, 20, 40, 30, 60, 10, 50, 20, 40, 30, 60, 10, 0]


def main():
    ser = proto.connect()
    sleep(1)

    eyes_open = [(c, proto.VALUE_MAX) for c in EYES]
    eyes_closed = [(c, 0) for c in EYES]

    frames = []
    for i, pct in enumerate(MOUTH_POSITIONS):
        frames.append(proto.encode_frame([(MOUTH, proto.pct_to_value(pct))]))
        if i % 3 == 0 and i != 0:
            frames.append(proto.encode_frame(eyes_closed))
            frames.append(proto.encode_frame(eyes_open))

    for f in frames:
        ser.write(f)
        sleep(0.01)

    ser.write(bytes([proto.PLAY]))
    ser.close()


if __name__ == '__main__':
    main()
