# Offline demo: the remote control

**Start here.** This is the program to learn on.

It does one job: you type a number into the Arduino's serial monitor and a servo
moves to that position. No computer, no Python, no timing. You are the animation
system.

It is also the calibration tool, and the only program in this repository that
builds today. The other one,
[`roboface-streaming/`](../roboface-streaming/README.md), is a work in progress
that does not compile.

Read [`../README.md`](../README.md) first for the hardware, the wiring, and what
a servo actually is. This page assumes you have that.

---

## The files

| File | What it is |
|---|---|
| `roboface-setup.ino` | The whole program. |
| `features.h` | Which channel each feature is on, and its open/closed positions. **The important file.** |
| `positions.h` | Two preset commands: `EYES_CLOSED` and `EYES_OPEN`. |
| `actions.h` | The `talk` and `blink` commands, and the mouth movement table behind `talk`. |

The sketch is derived from Adafruit's demo for their PWM servo board, so if you
have seen that one, this will look familiar. Read
[`../README.md#how-a-servo-actually-works`](../README.md#how-a-servo-actually-works)
if you have not.

---

## Load and flash it

You only do steps 1 to 4 once. After that, when you edit the code, you go
straight to step 4.

**1. Install the library.** In the Arduino IDE, *Sketch → Include Library →
Manage Libraries...*, search for `Adafruit PWM Servo Driver Library`, and click
Install. It is the only library this sketch needs.

**2. Open the sketch.** *File → Open*, then pick `roboface-setup.ino`. The IDE
opens the folder containing it, which is what you want.

**3. Pick the board.** *Tools → Board → Arduino Uno*. Then check
*Tools → Port* and pick the one your Uno is on. Getting the board right matters:
the IDE needs it to pick the right chip settings.

**4. Upload.** Click the **right-arrow button** (or *Sketch → Upload*). The
lights will blink, the IDE will print `Done compiling` and `Done uploading`, and
the sketch will restart. If the IDE stops with a red error box, that error is the
most useful thing on screen. Read it; it usually names the line.

Then, and only then, plug the servo power supply in.

---

## Run it

*Tools → Serial Monitor*. Two settings, both of which will waste your afternoon
if you get them wrong:

- **Baud rate: 9600.** The other sketch in this repo uses 115200. This one uses
  9600.
- **Line ending: Newline.** This is the dropdown at the bottom-right of the
  Serial Monitor. If it is set to "No line ending", nothing you type will ever do
  anything, and it will look exactly like a broken board.

The monitor should print this when you open it:

```
8 channel Servo test!
```

That is a leftover message from Adafruit's original demo. It is inaccurate, the
board has 16 channels, and nothing is wrong.

---

## Commands

Type a number, press enter, the face moves.

| Type | Meaning |
|---|---|
| `1` to `16` | **Select** a channel. The number is 1-based, so you type `1` for channel 0, which is the mouth. |
| `150` to `600` | Move the **selected** channel to that pulse count. |
| `101` | `EYES_CLOSED`. All four eyelids at once. |
| `102` | `EYES_OPEN`. All four eyelids at once. |
| `1001` | `BLINK`. Closes the eyes for 100 ms, then opens them. |
| `1002` | `TALK`. Waggles the jaw for a couple of seconds. |

A worked example:

```
> 1          selects channel 0, the mouth
Set selected motor channel to 0
> 200        mouth closed
Set position of motor 0 to 200
> 450        mouth open
Set position of motor 0 to 450
> 4          selects channel 3, the right upper eyelid
Set selected motor channel to 3
> 280        eyelids shut
Set position of motor 3 to 280
> 1001       blink
```

The 1-based channel selection is a real trap and the one thing to double-check
first. `features.h` says `LEFT_EYE_UPPER_LID 1`, and that 1 is a channel number,
so you type `2` to select it.

### Numbers the sketch ignores

If you type something outside every range above, the sketch stays silent. It is
not broken, you just typed a number it has no meaning for. The dead zones are:

`0`, `17` to `100`, `103` to `149`, `601` to `1000`, and anything `1003` or above.

---

## Calibrating a servo

This is the real work. The numbers currently in `features.h` were measured on the
original servos with the original horns. Yours will be different. If the face
looks unsettlingly wrong, that is why.

**Read this part before you do it. A servo driven past its limit will strip its
gears, and the code cannot tell you that it is happening.**

The safe method, for the left lower eyelid as an example:

1. **Take the horn off if you can.** Or better, disconnect the servo from its
   horn link, so nothing is under load. It is much easier to find the limits with
   nothing pushing back.
2. Put the servo in the middle of the known range first. For this eyelid that is
   `280`, the "closed" number already in the file.
3. Nudge the number up by 5 or 10 at a time. `290`, `300`, `310`.
4. **Stop at the first sign of resistance.** A little tension is normal. Grinding,
   buzzing, or a servo that is warm is not. The moment the servo stops following
   smoothly, back off by 10 and call that your number.
5. Now do the same in the other direction, down to about `190`, and write that
   number into the `..._OPEN` entry.
6. Reconnect the linkage. If the range you found is now correct but the lid moves
   the wrong way as you increase the number, that is normal for a horn that is
   clipped on backwards. Either swap the two numbers in the file, or flip the
   horn. Both work; flipping the horn is usually kinder to the linkage.

Then do the other four. Mouth, left upper, left lower, right upper, right lower.

### Write it down in two places

**Both copies.** `features.h` exists twice, once in this directory and once in
[`../roboface-streaming/`](../roboface-streaming/README.md). They are not linked.
A calibration fix in one and not the other means the two programs will disagree
about what 280 means, and you will find that out weeks from now.

`actions.h` and `positions.h` are byte-identical copies for the same reason. Keep
it that way, or note the difference.

### What the numbers in the file mean

| Line in `features.h` | Count | Rough pulse length |
|---|---|---|
| `MOUTH_CLOSED 200` | 200 | about 0.98 ms |
| `MOUTH_OPEN 450` | 450 | about 2.20 ms |
| `LEFT_EYE_UPPER_LID_OPEN 240` | 240 | about 1.17 ms |
| `LEFT_EYE_LOWER_LID_OPEN 330` | 330 | about 1.61 ms |
| `RIGHT_EYE_UPPER_LID_OPEN 320` | 320 | about 1.56 ms |
| `RIGHT_EYE_LOWER_LID_CLOSED 280` | 280 | about 1.37 ms |
| `RIGHT_EYE_LOWER_LID_OPEN 190` | 190 | about 0.93 ms |

Full table in [`../README.md#what-the-numbers-mean`](../README.md#what-the-numbers-mean).

You will notice that the upper lids are **backwards**: "open" is the lower number.
The horn on those servos is flipped, so the lid sweeps the other way as the count
goes up. The file describes what the hardware does, and that is the correct
behavior for it to have.

---

## How the code works

Four ideas, and that is the whole program.

**`setup()` runs once, `loop()` runs forever.** This is every Arduino sketch.
Everything in `setup()` happens at startup; `loop()` is where the program lives.

**`Serial.available()` asks "has anything arrived?"** In the Serial Monitor you
type a number, it goes out the USB cable as text, and it lands in a buffer in the
Uno. `Serial.available()` returns how many characters are sitting there. If it is
bigger than zero, there is something to do.

**`Serial.parseInt()` reads a whole number out of that buffer.** You type `450`
and it hands back the integer 450. It waits for a newline, which is why the line
ending setting matters.

**A chain of `if` / `else if` decides what the number meant.** Read the chain in
`roboface-setup.ino` from the top. It tests, in order: is this a channel number?
is this a preset? is this a raw position? is this an action? Whichever test is
true first wins, and the rest are skipped. That is why `101` is a preset and not
a position, and why there is a gap between `102` and `150`.

Two helpers worth reading, because they are most of the interesting code:

- **`map()`** turns one range of numbers into another.
  `map(talk_map[i][0], 0, 100, MOUTH_CLOSED, MOUTH_OPEN)` means "I have a
  percentage from 0 to 100, give me the pulse count for that percentage of the
  mouth's range". It is the standard Arduino function and it is genuinely useful.

- **`talk_map`**, in `actions.h`, is a **two-dimensional array**: five rows, two
  numbers each. Each row is `{ how open, how long in milliseconds }`.

  ```c
  int talk_map[5][2] = {
    { 50, 100 },   // half open, for a tenth of a second
    { 10, 100 },   // nearly closed
    { 30, 100 },
    { 10, 100 },
    { 50, 100 }
  };
  ```

  The first number is a percentage of the mouth's travel, and the second is how
  long to sit there. `talk()` walks that list five times over. **That is the whole
  "animation" system, and it is a table you can edit.** Change the numbers and
  you have a different mouth movement.

---

## Rough edges

Honest list of things that are odd, so you do not lose an hour to them.

- **`setServoPulse()` is never called.** It is inherited from Adafruit's demo and
  unused. It also prints a lot if you ever do call it.
- **`USMIN` and `USMAX` are defined but never used**, and they are leftovers from
  Adafruit's demo. Their comments are still the best explanation in the file of
  what the counts mean, so leave them. `SERVOMIN` and `SERVOMAX` are used once, at
  line 88, and they are not limits on your servos. They are just the edges of the
  range the raw-position command accepts, which is why typing `100` or `900` does
  nothing.
- **`bool move = true;` is unused.** Dead variable.
- **The startup banner says "8 channel Servo test!".** Wrong on both counts.
  Ignore it.
- **`blink()` and `talk()` block.** `delay()` freezes the entire program, so
  during a 2.5 second `talk()` the Serial Monitor will not respond and you cannot
  interrupt it. That is a limitation of this simple version and the main reason
  the streaming half exists.
- **The eye presets ignore the mouth.** `101` and `102` move the four lids only.
- **The sketch stores nothing.** There is no memory of the positions you set. The
  servos themselves will sit wherever you left them, because that is how a servo
  works, but re-upload the sketch and you are starting from `features.h` again.

---

## Things to try

In rough order of how much they teach you.

1. **Retune `talk_map`.** Make the mouth chatter faster, or make `talk()` last
   twice as long by adding another loop. This is a five-number edit.
2. **Add a preset.** Suppose you want a "perked" look, eyes open and jaw slightly
   open. Add `#define PERKED 103` to `positions.h` in both directories, then add
   an `else if` branch in the preset section of `loop()` that moves all four lids
   and the mouth. Remember the 1-based channel thing: `MOUTH` is `0`, so you
   address it as `pwm.setPWM(0, 0, ...)`.
3. **Add an action.** `WAVE` on a channel nobody uses yet, or an `EARPLUGS`
   command that closes the eyes and then twitches the mouth a few times. Add the
   number to `actions.h` in both directories, then a branch in the action
   section. The numbers in the 1000s are reserved for actions, so 1003 is free.
4. **Make a real expression.** Four or five commands in a row, typed by hand,
   make a surprisingly convincing face. A blink, a slow blink, a `talk`, an
   `EYES_CLOSED`. This is the thing animatronics is actually made of.
5. **Add a servo.** Eyebrows. The driver board has 16 channels and you are using
   5. Add a `#define` for the new feature and its channel in **both** copies of
   `features.h`, wire it in, and calibrate it the same way. Do not change the
   order of the existing five, or you will repoint every servo at once.

When you are comfortable with all of that, read
[`../roboface-streaming/README.md`](../roboface-streaming/README.md). It is the
same face, driven by a computer instead of your fingers, and it is where
expressions stop being typed one at a time and start being played as a sequence.
