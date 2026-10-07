# Streaming: the live driver

This is the real roboface. A computer on the other end of the USB cable sends it
frames of positions, and the face glides from one to the next over a fixed amount
of time. That is how you get a blink that looks like a blink instead of a slap.

It replaces the offline demo in
[`../roboface-offline-demo/`](../roboface-offline-demo/README.md) rather than
running alongside it. Same face, same servos, same five channels. The difference
is where the instructions come from: your fingers on a keyboard, versus a script.

## It compiles now

This sketch is a working driver: it receives length-prefixed frames over serial
and tweens the face between them. Build it with the Arduino IDE (**Tools → Board
→ Arduino Uno**) after installing the libraries listed below.

---

## The files

| File | What it is |
|---|---|
| `roboface-streaming.ino` | The whole program: serial parsing, the frame queue, playback. |
| `Feature.h` / `Feature.cpp` | A class. One of these per servo. Knows its channel, its limits, and how to move itself. |
| `features.h` | Which channel each feature is on, and its open/closed positions, plus `featureMap` and `featureCount`. **The important file.** |
| `positions.h` | Copy of the offline demo's. Not included by this sketch. |
| `actions.h` | Copy of the offline demo's. Not included by this sketch. |
| `serial_protocol.py` | Shared wire helpers: control bytes, frame encoding, port selection. |
| `driver.py` | Interactive CLI. Build frames by hand and transmit them. |
| `playback.py` | A scripted jaw movement with automatic blinking. |
| `live_audio.py` | Drives the mouth from how loud the room is. Reads the microphone. |
| `send_serial.py` | A scratch file for poking at the serial port. Not a tool. |

Everything about hardware, wiring, and what a servo is lives in
[`../README.md`](../README.md). This page assumes you have read that.

### The duplicated files

`actions.h` and `positions.h` are **byte-identical copies** of the ones in the
offline demo. `features.h` is a near-copy with two extra things: `featureCount`
and `featureMap`. There is no shared include folder, so a calibration change has
to be made in both directories. This is a known annoyance, not a design choice
you should imitate.

---

## Install the libraries

More than the offline demo needs. All from *Sketch → Include Library → Manage
Libraries...* in the Arduino IDE:

| Search for | Used by |
|---|---|
| `Adafruit PWM Servo Driver Library` | the driver board |
| `TweenDuino` | smooth motion. Brings in `Tween` and `Timeline`. |
| `ArduinoQueue` | the frame queue |

Then **Tools → Board → Arduino Uno**, and upload.

---

## How it is supposed to work

### The idea: frames

A **frame** is a snapshot of all five servos at once, one number per feature. Five
numbers: mouth, left upper lid, left lower lid, right upper lid, right lower lid.

The computer sends frames as fast as it likes. The faceboard does not play them
immediately. It puts them in a **queue**, which is a line of things waiting their
turn, and it works through them one at a time, one every `time_per_frame`
milliseconds (1000 by default, so one second per frame).

A **tween** is what makes the motion smooth. Instead of jumping the servo from
wherever it is to the new number, a tween works out what the position should be
right now, partway through, and updates it. That is why the face glides. The
TweenDuino library does the arithmetic; you never write it.

```
   computer                 the face
       |                        |
   frame 1  ----------------> queued
   frame 2  ----------------> queued
   frame 3  ----------------> queued
       |
                  PLAY  <---- "start moving"
                    |
              tween to frame 1 over 1 second
              tween to frame 2 over 1 second
              tween to frame 3 over 1 second
                    |
              queue is empty, so it stops
```

### What a frame costs in memory

One frame is one byte per feature, so five bytes today. There are two arrays of
100 frames: the queue and `savedFrames`. That is 500 bytes each, 1000 bytes of the
Uno's 2048 bytes of RAM, before anything else. Adding features grows both arrays
at the same time, because a frame gets one byte longer. Sixteen features and a
hundred saved frames would need 3200 bytes, and will not fit. If you add a lot of
servos, either shorten those two arrays or move to a board with more RAM.

### The unit: 0x00 to 0xff

This is the part that differs from the offline demo, and the part worth
understanding before touching anything.

The offline demo takes **raw pulse counts**, the 150-to-600 numbers that map
directly onto servo pulses. This sketch takes a **normalized** number from 0 to
255 instead, and each feature scales that into its own range.

So `0xff` does not mean a particular pulse length. It means **"as open as this
feature can go"**, and the firmware works out what that is for that particular
servo:

```c
int Feature::mapMe(int to) {
  return map(to, 0, 255, minPos, maxPos);
}
```

`minPos` and `maxPos` come from `featureMap` in `features.h`, and every row of
that table is `{ CLOSED, OPEN }`:

```c
int featureMap[5][2] = {
  {MOUTH_CLOSED, MOUTH_OPEN},                     // 200 -> 450, forwards
  {LEFT_EYE_UPPER_LID_CLOSED,LEFT_EYE_UPPER_LID_OPEN},   // 280 -> 240, backwards
  {LEFT_EYE_LOWER_LID_CLOSED,LEFT_EYE_LOWER_LID_OPEN},   // 280 -> 330, forwards
  {RIGHT_EYE_UPPER_LID_CLOSED,RIGHT_EYE_UPPER_LID_OPEN}, // 280 -> 320, forwards
  {RIGHT_EYE_LOWER_LID_CLOSED,RIGHT_EYE_LOWER_LID_OPEN}, // 250 -> 190, backwards
};
```

Three of those five rows run backwards, because those horns are clipped on
flipped. The order of the columns is what matters, not the order of the numbers,
and `map()` handles the inversion without being told. The useful consequence:

> **`0xff` always opens a feature fully. `0x00` always closes it fully. Always, on
> all five features, no exceptions.**

That is worth more to a Python script than knowing the actual counts, which is
why `driver.py` and friends never mention the hardware at all.

The third unit, if you count to three, is **percent, 0 to 100**, which is what the
Python scripts speak. They convert it themselves.

### `Feature`, the class

One `Feature` per servo, in an array of five, set up in `setup()` from
`featureMap`. A `Feature` knows:

- its `channel`, which output on the driver board it is plugged into
- its `minPos` and `maxPos`, the raw counts for closed and open
- its `pwm`, a pointer to the driver board, so it can move itself
- a `tween`, which is what makes it move smoothly

The methods you will use:

| Method | What it does |
|---|---|
| `setAll(pwm, chan, min, max)` | Tell it everything. Called once at startup. |
| `setTarget(to, ms)` | Tween to a `0x00`-`0xff` position over `ms` milliseconds. |
| `updatePWM()` | Ask the tween where we are now, and write it to the servo. Called every loop. |

Two design notes that will save you time later:

**The loop does the work.** `updatePWM()` is called for all five features on
every pass through `loop()`, but only while `playing` is true. When the face is
idle, the firmware is not writing to the servos at all. That is deliberate: the
servos hold their own position, and the I2C traffic is not free.

**A `Feature` allocates.** `setTarget()` frees the old tween and makes a new one,
so a full-speed frame costs five allocations and frees. At one frame per second
that is nothing. If you ever shorten `time_per_frame` a lot, it becomes a
reason to be careful.

---

## The serial protocol

Baud rate **115200**. 8 data bits, no parity, 1 stop bit.

A **frame** is a `0xff` marker, a one-byte count `N` of `(channel, value)` pairs,
then `N` pairs. Control bytes stand alone.

| Byte | Meaning |
|---|---|
| `0xff` | **Frame start.** The next byte is the pair count. |
| `N` | Number of `(channel, value)` pairs that follow. |
| `channel` | Feature index, 0 to 4. Range-checked against `featureCount`. |
| `value` | 0 to 255. Mapped onto the feature's own min/max. |
| `0xfe` | **PLAY.** Start moving. |
| `0xfd` | **STOP.** Stop moving. |
| `0xfb` | **START_DL.** Enter download mode. |
| `0xfc` | **STOP_DL.** End download, enter playback. |

A complete five-feature frame is the marker, the count, and five pairs:

```
0xFF 0x05          frame start, 5 pairs
0x00 0xE0          mouth            to 224
0x01 0xFF          left upper lid   to 255, wide open
0x02 0x00          left lower lid   to 0, closed
0x03 0xFF          right upper lid  to 255
0x04 0xFF          right lower lid  to 255
```

Because the count tells the parser exactly how many payload bytes to read, a
value of `0xff` inside a frame is never mistaken for a marker, so **every value
is representable**. A stray `0xff` when no frame is in progress gives a natural
resync point: the parser reads the count, and if `N > featureCount` it discards
one byte and resyncs.

---

## The three modes

The sketch has three operating modes, switched by the wire bytes above.

- **STREAMING** (default): frames are enqueued and played in order.
- **PLAYBACK_DL**: `START_DL` (`0xfb`) enters it; frames are saved to `savedFrames`.
- **PLAYBACK**: `STOP_DL` (`0xfc`) ends download and enters it; `PLAY` (`0xfe`)
  plays the saved frames back from the start.

---

## Run it from Python

The four `.py` files are **senders**. They share `serial_protocol.py` for the
wire format and port selection, and each is guarded with
`if __name__ == '__main__'`, so importing one no longer starts it. Only one can
hold the port at a time, so only one can run at a time.

### Set up

```
python -m pip install pyserial
```

`live_audio.py` additionally needs `pyaudio` and `numpy`. Those are harder to
install than they should be, because PyAudio has to build PortAudio first. On
Windows this is a known annoyance. Treat `live_audio.py` as a later project.

### Before anything else

- **Close the Arduino Serial Monitor.** It holds the port and your script will
  fail to open it with a permission error. This is the single most common
  failure.
- **Check the port name.** The port defaults to `COM4` and can be overridden
  with the `ROBOFACE_PORT` environment variable. If your Uno is on `COM3` or
  `COM7`, find out from the Arduino IDE's *Tools → Port* menu and set
  `ROBOFACE_PORT` accordingly.
- **Upload the firmware first.** These scripts do nothing without it.

### `driver.py` — build frames by hand

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

### `playback.py` — a scripted performance

A hardcoded list of twenty mouth positions that walks up and down, with the eyes
blinking every few frames. It sends each position as a proper length-prefixed
frame, then sends `PLAY`, so on its own it moves the face. It is a demo of what
the protocol looks like from the outside, not a finished piece.

### `live_audio.py` — the mouth follows the room

Opens the microphone, measures how loud it is, and sends that as the mouth
position. It is the prototype for a face that reacts to being spoken to, which is
a real animatronics skill and the most interesting idea in this directory.

It reads only channel 0, so it only ever moves the mouth. Note the frame-rate
mismatch: it sends about ten times a second while the firmware plays one frame
per `time_per_frame`, so most frames are dropped; treat it as a sketch, not a
finished driver.

### `send_serial.py` — a scratch file

Writes one valid frame (mouth and one eyelid), then prints whatever came back.
It exists to prove the port opens, nothing more. Do not build on it.

---

## Known gaps

Not bugs to report. Design notes on where this stands.

**Frames are dropped when the host is fast.** The queue holds 100 frames. Send
more than that faster than `time_per_frame` and `enqueue()` fails silently. The
face plays the first 100 and skips the rest with no error anywhere.

**Playback stops when its source empties.** `playing` goes false when the queue
(or `savedFrames`) drains, and a host that is still sending has to send `PLAY`
again. It is defensible; it is just a thing to know.

**A frame leaves unset channels at zero.** `next_frame = frame()` value-initializes
the struct, so any channel a frame does not mention is `0`, which is fully closed.
`driver.py` skips unset features, so an eye you did not set snaps shut. Worth
knowing before it looks like a bug.

**`DEBUG` is off by default.** Uncomment `#define DEBUG` at the top of the sketch
to log received bytes to the serial port. It is a lot of noise, but a useful
window into the protocol; the Serial Monitor's two directions are independent, so
it does not corrupt the wire.

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
4. **Raise the frame rate.** Shorten `time_per_frame` and find the point where
   frames start getting dropped, then work out the limit and the fix. There is
   arithmetic in that, and it is the arithmetic of the whole design.
5. **Use the playback modes.** Send `START_DL`, a few frames, `STOP_DL`, then
   `PLAY`, and watch the saved frames play back.
