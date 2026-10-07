import serial_protocol as proto

FEATURES = [
    'Mouth',
    'Left eyelid U',
    'Left eyelid L',
    'Right eyelid U',
    'Right eyelid L',
]


def new_frame():
    return [None] * len(FEATURES)


def print_frame(f, current):
    for i, name in enumerate(FEATURES):
        mark = '*' if i == current else ' '
        print('{} {}: {}'.format(mark, name, f[i]))


def encode_frames(frames):
    out = bytearray()
    for f in frames:
        pairs = [(i, proto.pct_to_value(f[i])) for i in range(len(FEATURES)) if f[i] is not None]
        out += proto.encode_frame(pairs)
    return bytes(out)


def main():
    ser = proto.connect()
    frames = []
    current_frame = new_frame()
    current_feature = 0

    print('''Interactively build frames and send them to the face.
Set each feature as a percentage (0-100). Commands:
  <number>  set the current feature and advance
  s         skip to the next feature
  b         go back to the previous feature
  f         finish this frame and start a new one
  t         transmit all buffered frames
  p         send PLAY
  x         quit
''')

    while True:
        print('{} frames in buffer.\n'.format(len(frames)))
        print_frame(current_frame, current_feature)

        if current_feature < len(FEATURES) - 1:
            cmd = input('(s)kip, (b)ack, (f)inish frame, e(x)it >> ')
        else:
            cmd = input('(f)inish frame >> ')

        if cmd.startswith('s'):
            if current_feature >= len(FEATURES) - 1:
                print('already on the last feature')
                continue
            current_feature += 1
        elif cmd.startswith('b'):
            if current_feature <= 0:
                print('already on the first feature')
                continue
            current_feature -= 1
        elif cmd.startswith('f'):
            frames.append(current_frame)
            current_feature = 0
            current_frame = new_frame()
        elif cmd.startswith('x'):
            ser.close()
            return
        elif cmd.startswith('t'):
            barr = encode_frames(frames)
            print(' '.join('{:02x}'.format(b) for b in barr))
            ser.write(barr)
            print('done.')
        elif cmd.startswith('p'):
            print('Sending PLAY')
            ser.write(bytes([proto.PLAY]))
        else:
            try:
                i = int(cmd)
                if 0 <= i <= 100:
                    current_frame[current_feature] = i
                    current_feature += 1
                else:
                    print('positions should be 0-100')
            except ValueError:
                print("I don't know what to do with that.")


if __name__ == '__main__':
    main()
