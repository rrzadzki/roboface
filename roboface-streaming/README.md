# Streaming: the live driver

This is the real roboface. A computer on the other end of the USB cable sends it
frames of positions, and the face glides from one to the next over a fixed amount
of time. That is how you get a blink that looks like a blink instead of a slap.

It replaces the offline demo in
[`../roboface-offline-demo/`](../roboface-offline-demo/README.md) rather than
running alongside it. Same face, same servos, same five channels. The difference
is where the instructions come from: your fingers on a keyboard, versus a script.

## It does not compile yet. Read this first.

As committed, the Arduino IDE will refuse to upload this sketch. Five identifiers
are used but never defined, one parenthesis is missing, and one line uses `==`
where it means `=`. It is a half-finished refactor, not a broken build.

This is normal for a project mid-flight, and the fix list is written out below in
[`Fix it until it compiles`](#fix-it-until-it-compiles), with a line number and an
explanation for each item. It is a genuinely good first programming exercise: the
error messages from the IDE will tell you the same things, and reading them is the
skill.

Until it compiles, use the
[offline demo](../roboface-offline-demo/README.md) for anything involving the
hardware.

---

## The files

| File | What it is |
|---|---|
| `roboface.ino` | The whole program: serial handling, the frame queue, playback. |
| `Feature.h` / `Feature.cpp` | A class. One of these per servo. Knows its channel, its limits, and how to move itself. |
| `features.h` | Which channel each feature is on, and its open/closed positions, plus `featureMap` and `featureCount`. **The important file.** |
| `positions.h` | Copy of the offline demo's. `EYES_CLOSED`, `EYES_OPEN`, and an unused `BORED`. |
| `actions.h` | Copy of the offline demo's. `talk_map` is not used by this sketch. |
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

Then **Tools → Board → Arduino Uno**, and upload. It will fail. See below.

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
   0xff "end of frame"
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

### The unit: 0x00 to 0x7f

This is the part that differs from the offline demo, and the part worth
understanding before touching anything.

The offline demo takes **raw pulse counts**, the 150-to-600 numbers that map
directly onto servo pulses. This sketch takes a **normalized** number from 0 to
127 instead, and each feature scales that into its own range.

So `0x7f` does not mean a particular pulse length. It means **"as open as this
feature can go"**, and the firmware works out what that is for that particular
servo:

```c
int Feature::mapMe(int to) {
  return map(to, 0, 0x7f, minPos, maxPos);
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

> **`0x7f` always opens a feature fully. `0x00` always closes it fully. Always, on
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
| `setPos(pos)` | Jump immediately to a position. Broken, see below. |
| `setTarget(to, ms)` | Tween to a `0x00`-`0x7f` position over `ms` milliseconds. |
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

One byte at a time. The **high bit** of each byte says how to read it, and the
firmware strips that bit off to get the payload. It is a small, tidy protocol and
worth learning on, because you can drive the whole face from a text editor that
can send hex bytes if you ever want to.

| Byte | Meaning |
|---|---|
| `0x00` to `0x7f` | A **channel** number, 0 to 127. Remembers which servo the next value is for. |
| `0x80` to `0xff` | A **value** for the last channel named, 0 to 127 once the high bit is stripped. |
| `0xff` | **End of frame.** Queue everything received since the last end-of-frame. |
| `0xfe` | **PLAY.** Start moving. |
| `0xfd` | **STOP.** Stop moving. |

A frame is therefore channel, value, channel, value, ..., `0xff`. The most
complete frame is ten bytes plus the terminator:

```
0x00 0xF0    mouth  to 112
0x01 0xFF    left upper lid  to 127, wide open
0x02 0x80    left lower lid  to 0, closed
0x03 0xFF    right upper lid to 127
0x04 0xFF    right lower lid to 127
0xFF         end of frame
```

### Four things that will surprise you

**Values 0x7d to 0x7f can never be sent.** The three control bytes are
indistinguishable from value bytes. `0xff` is checked before the value branch, so
it is always a terminator and never a value, which means 127 is unreachable.
Percent 98, 99 and 100 are the numbers you can never express.

**`driver.py` puts percent 100 on the PLAY byte.** It maps 0 to 100 onto
`0x00` to `0x7e`, so the top of the scale is `0x7e | 0x80` = `0xfe` = PLAY. A
frame with a feature at 100 will start the face moving as a side effect. Not
fatal, but confusing the first time you see it. `playback.py` maps onto `0x00` to
`0x7f` instead, so there percent 100 sends an end-of-frame. Two scripts, two
different off-by-one mistakes, opposite directions.

**The read is not resynchronized.** The firmware asks for exactly two bytes at a
time with `readBytes()`. If a short read happens mid-frame, every channel and
value after that point is paired with the wrong partner, and it will not recover
until the next frame. Slow networks and long cables both make this more likely.

**A channel number is not range-checked.** The wire allows channels 0 to 127 but
there are only five features. Send channel 9 and the firmware writes past the end
of the array. See [Known gaps](#known-gaps).

---

## Fix it until it compiles

Six edits, in this order. The IDE's error messages will point at the same lines.

**1. The mode identifiers are missing.** `MODE` is read at lines 120, 163 and
203. `STREAMING`, `PLAYBACK_DL` and `PLAYBACK` are compared against at 120, 163
and 203. `STOP_DL` is compared at 165. None of the five exist. Add them near the
top of `roboface.ino`, an enum being the obvious shape:

```c
enum Mode { STREAMING, PLAYBACK, PLAYBACK_DL, STOP_DL };
Mode MODE = STREAMING;
```

`STOP_DL` reads oddly as a mode and probably was meant to be a wire byte rather
than a mode. Either way, something has to be true of it for line 165 to work.

**2. Line 165 is missing a closing parenthesis.** `if((buffer[i] == STOP_DL) {`
has two opening parens and one closing. It should be:

```c
if(buffer[i] == STOP_DL) {
```

**3. Line 166 compares instead of assigning.** `MODE == PLAYBACK;` asks a
question and throws the answer away. This is the transition out of download mode
and into playback, so it needs `=`:

```c
MODE = PLAYBACK;
```

**4. `nextFrameIdx` does not exist.** Line 173 writes to
`savedFrames[nextFrameIdx]` and nothing ever declares it. It needs to start at 0
and advance, and it needs a bound, because `savedFrames` holds 100 entries and
nothing currently stops it writing past that:

```c
if(nextFrameIdx < 100) {
  savedFrames[nextFrameIdx++] = next_frame;
}
```

Silently dropping the overflow is the quick fix. Writing past the end of a
100-element array is the kind of bug that looks like nothing happening for a
while and then corrupts unrelated memory.

**5. `PLAYBACK` and `PLAYBACK_DL` are mutually exclusive branches.** They are
`else if`s on the same variable, so only one can ever run per byte. That is
probably intentional, since 166 moves the mode to `PLAYBACK` and the next byte
should indeed be handled by the playback branch. But it is worth a comment
explaining it, because the next person to read it will assume it is a bug.

**6. `#include <TweenDuino.h>` appears twice**, at lines 4 and 17. Harmless,
since includes are guarded, but delete one.

After that it should compile. If it does not, the error is somewhere in
`Feature.cpp` and it will say so.

---

## Run it from Python

The four `.py` files are **senders**. They are not a library, they are four
separate little programs, and **each one opens the serial port the moment Python
starts it**. Only one can hold `COM4` at a time, so only one can run at a time.

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
- **Check the port name.** Every script hardcodes `COM4`. If your Uno is on
  `COM3` or `COM7`, find out: the Arduino IDE's *Tools → Port* menu shows what it
  is, and the file you need to change is line 7 of whichever script you are
  running. The port is hardcoded in all four, with no flag or environment
  variable.
- **Upload the firmware first.** These scripts do nothing without it.

### `driver.py` — build frames by hand

The most useful of the four, and the one to start with. It is a prompt loop, and
it starts as soon as Python imports the file.

```
50          set the current feature to 50% and move to the next one
s           skip to the next feature without setting it
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

Two things to know about it. Values are percent, 0 to 100. And you can only move
forward: there is no way to go back and fix a value you got wrong, so double-check
before you press `f`.

### `playback.py` — a scripted performance

A hardcoded list of twenty mouth positions that walks up and down, with the eyes
blinking on a counter. It is a demo of what the protocol looks like from the
outside, not a finished piece. Two things to know: it never sends `0xff`, so it
never actually ends a frame, and it never sends `PLAY` either, so on its own it
will not move the face. It is also the only file that shows a longer sequence,
which makes it worth reading for the shape of one.

### `live_audio.py` — the mouth follows the room

Opens the microphone, measures how loud it is, and sends that as the mouth
position about ten times a second. It is the prototype for a face that reacts to
being spoken to, which is a real animatronics skill and the most interesting idea
in this directory.

It reads only channel 0, so it only ever moves the mouth, and it never sends
`PLAY`, so the face will not move until something else starts it. `audioop` was
removed from Python in 3.13, so on a current Python this file errors out on
import. Both of those are small fixes.

### `send_serial.py` — a scratch file

Four test bytes, a write, and a print of whatever came back. The bytes it computes
are `0`, `32`, `32`, `0`, and channel 32 is out of range for five features. It
exists to prove the port opens, nothing more. Do not build on it.

---

## Known gaps

Not bugs to report. Design notes on where this stands.

**`setTarget()` currently has no visible effect.** This is the big one.
`Feature::updatePWM()` writes `Feature::pos` to the servo, but `pos` is never
updated from the tween:

```c
void Feature::updatePWM() {
  tween->update(millis());
  pwm->setPWM(channel, 0, pos);   // always the old pos
}
```

The tween is doing the work and the result is being thrown away. In TweenDuino,
`update()` returns the interpolated value, so the fix is to assign it, and to
store it in a variable wide enough to hold the interpolated result:

```c
void Feature::updatePWM() {
  pos = tween->update(millis());
  pwm->setPWM(channel, 0, pos);
}
```

`pos` is a `float` and raw counts fit in about 12 bits, so the rounding here
matters more than it looks. Check the return type of `update()` in the version of
TweenDuino you installed before trusting that line. Reading back whatever the servo
actually did, rather than what the tween thinks it did, is the first thing to fix
after it compiles.

**`setAll()` double-maps its minimum.** `pos = mapMe(minPos)` takes a value that
is *already* a raw count and runs it through a function that expects a 0x00 to
0x7f value. It produces a number well outside the servo's range. The constructor
`Feature(pwmD, chan, min, max)` gets this right with `pos = minPos`, so the two
entry points into the same object disagree with each other. `setAll()` should be
matching the constructor.

**`setPos()` is broken twice.** It range-checks against `minPos`/`maxPos`, which
means it can never succeed for the three features whose range is inverted, since
`p >= 280 && p <= 240` is never true. And when the check does pass, it maps the
value a second time, the same mistake as `setAll()`. It is also called with
`0x7f` in `setup()`, which fails the range check on all five features, so right
now it does nothing at all. `setPos()` is not part of the working path; tweening
is. Either fix it or delete it.

**Frames are dropped when the host is fast.** The queue holds 100 frames. Send
more than that faster than `time_per_frame` and `enqueue()` fails silently. The
face plays the first 100 and skips the rest with no error anywhere.

**Playback stops when the queue empties.** `playing` goes false, and a host that
is still sending has to send `PLAY` again. This is at the end of `loop()`. It is
defensible, and it is also the reason `playback.py` appears to do nothing.

**A frame leaves unset channels at zero.** `next_frame = frame()` value-initializes
the struct, so any channel a frame does not mention is `0`, which is fully closed.
`driver.py` skips unset features, so an eye you did not set snaps shut. Worth
knowing before it looks like a bug.

**`DEBUG` is on.** Line 13. The firmware logs every byte it receives, in detail,
to the same serial port the protocol uses. It does not break anything, since that
port's two directions are independent, and the Serial Monitor is a genuinely
useful window into the protocol. But it is a lot of noise, and commenting it out
makes the port quiet for the Python scripts.

**`channel_indicator` is unused.** Line 61. Declared and never read.

**`BORED` is unused.** In `positions.h`, along with the rest of that file.

**`talk_map` is unused here.** `actions.h` is carried over from the offline demo
and this sketch never reads it.

---

## Things to try

Once it compiles and `driver.py` sends a frame you can see.

1. **Verify a single frame by hand.** Open *Tools → Serial Monitor* at 115200 and
   send `0 127` followed by `0xff`, in hex mode. The mouth should open. The
   Serial Monitor is an acceptable stand-in for Python and removes a whole layer
   of debugging.
2. **Fix `updatePWM()`** and watch the difference between a snap and a glide.
   This is the single most satisfying change in the directory.
3. **Two features at 100, 0, and 50**, on a lid and on the mouth, and watch for
   the frames that never end.
4. **Send 100 for a feature in `driver.py`** and work out why the face starts
   moving. Then decide whether to change the script's range or to change the
   protocol, and write down which.
5. **Add an expression to `driver.py`** as a one-liner that transmits a blink and
   a talk in sequence. Now you are animating instead of posing, and the rest of
   this project is a matter of doing more of that.
6. **Raise the frame rate.** Shorten `time_per_frame` and find the point where
   frames start getting dropped, then work out the limit and the fix. There is
   arithmetic in that, and it is the arithmetic of the whole design.
