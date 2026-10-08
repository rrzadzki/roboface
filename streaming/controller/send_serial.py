from time import sleep

import serial_protocol as proto


def main():
    ser = proto.connect()
    sleep(1)

    test = proto.encode_frame([(0, 64), (1, 128)])
    ser.write(test)
    sleep(1)

    while ser.in_waiting > 0:
        print(ser.read(ser.in_waiting))


if __name__ == '__main__':
    main()
