# Streaming controller: the Python senders

These scripts run on your computer and send frames to the face over USB serial.
The firmware that receives them lives in
[`../firmware/roboface-streaming-firmware/`](../firmware/roboface-streaming-firmware/README.md),
and that README documents the wire protocol the scripts speak.

Everything about hardware, wiring, and what a servo is lives in
[the root README](../../README.md).

---

## The files

| File | What it is |
|---|---|
| `serial_protocol.py` | Shared wire helpers: control bytes, frame encoding, port selection. |
| `driver.py` | Interactive CLI. Build frames by hand and transmit them. |
| `playback.py` | A scripted jaw movement with automatic blinking. |
| `live_audio.py` | Drives the mouth from how loud the room is. Reads the microphone. |
| `send_serial.py` | A scratch file for poking at the serial port. Not a tool. |

The four senders share `serial_protocol.py` (they `import serial_protocol`, so
keep them in the same directory), and each is guarded with
`if __name__ == '__main__'`, so importing one does not start it. Only one can
hold the port at a time, so only one can run at a time.

Values are **percent, 0 to 100** in the scripts. They convert to the firmware's
0x00-0xff range themselves (`pct_to_value()` in `serial_protocol.py`).

---

## Set up

```
python -m pip install pyserial
```

`live_audio.py` additionally needs `pyaudio` and `numpy`. Those are harder to
install than they should be, because PyAudio has to build PortAudio first. On
Windows this is a known annoyance. Treat `live_audio.py` as a later project.

Run the scripts from this directory:

```
cd streaming/controller
python driver.py
```

### Before anything else

- **Close the Arduino Serial Monitor.** It holds the port and your script will
  fail to open it with a permission error. This is the single most common
  failure.
- **Check the port name.** The port defaults to `COM3` and can be overridden
  with the `ROBOFACE_PORT` environment variable. If your Uno is on `COM4` or
  `COM7`, find out from the Arduino IDE's *Tools → Port* menu and set
  `ROBOFACE_PORT` accordingly.
- **Upload the firmware first.** These scripts do nothing without it.

---

## `driver.py` — build frames by hand

The most useful of the four, and the one to start with. It is a prompt loop.

```
50          set the current feature to 50% and move to the next one
s           skip to the next feature without setting it
b           go back to the previous feature
f           finish this frame and start a new one
t           transmit every buffered frame
p           send PLAY
x           quit
```

Walk through it: set a number, press `s` to skip past anything you do not care
about, `f` when the five features are set, then `t` and `p`. It prints the byte
array before sending it, which is the fastest way to learn the protocol, and you
can copy that same byte array into a serial terminal instead of running the
script at all.

Values are percent, 0 to 100, mapped onto the full 0-255 range.

## `playback.py` — a scripted performance

A hardcoded list of twenty mouth positions that walks up and down, with the eyes
blinking every few frames. It sends each position as a proper length-prefixed
frame, then sends `PLAY`, so on its own it moves the face. It is a demo of what
the protocol looks like from the outside, not a finished piece.

## `live_audio.py` — the mouth follows the room

Opens the microphone, measures how loud it is, and sends that as the mouth
position. It is the prototype for a face that reacts to being spoken to, which is
a real animatronics skill and the most interesting idea in this directory.

It reads only channel 0, so it only ever moves the mouth. Note the frame-rate
mismatch: it sends about ten times a second while the firmware plays one frame
per `time_per_frame`, so most frames are dropped; treat it as a sketch, not a
finished driver.

## `send_serial.py` — a scratch file

Writes one valid frame (mouth and one eyelid), then prints whatever came back.
It exists to prove the port opens, nothing more. Do not build on it.

---

## Things to try

1. **Verify a single frame by hand.** Open *Tools → Serial Monitor* at 115200 and
   send, in hex mode, `FF 01 00 FF` (a one-pair frame: mouth to 255) followed by
   `FE` (PLAY). The mouth should glide open. The Serial Monitor is an acceptable
   stand-in for Python and removes a whole layer of debugging.
2. **Two features at 100, 0, and 50**, on a lid and on the mouth, and watch the
   glide.
3. **Add an expression to `driver.py`** as a one-liner that transmits a blink and
   a talk in sequence. Now you are animating instead of posing, and the rest of
   this project is a matter of doing more of that.
4. **Use the playback modes.** Send `START_DL`, a few frames, `STOP_DL`, then
   `PLAY`, and watch the saved frames play back. The modes are described in the
   [firmware README](../firmware/roboface-streaming-firmware/README.md#the-three-modes).
