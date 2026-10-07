# ol' roboface

Firmware for a five-servo animatronic face. A mouth, two upper eyelids, two lower
eyelids. An Arduino Uno talks over a wire to a servo driver board, and the driver
board moves the servos.

There are **two separate programs** in this repository. They are two different
ways to control the same face:

| | |
|---|---|
| **[`roboface-offline-demo/`](roboface-offline-demo/README.md)** | A simple remote control. You type numbers in a serial monitor and the face moves. **Start here.** It builds today. |
| **[`roboface-streaming/`](roboface-streaming/README.md)** | The real thing. A computer sends timed frames over serial and the face glides between them. |

If you have never used a programmable board before, do the offline demo first. It
will teach you almost everything you need. Then, when it is working, come back
here and read why the streaming half exists.

---

## The parts you need

| Part | What it is | Notes |
|---|---|---|
| Arduino Uno | The brain. A small board with a microcontroller. | You program it over the USB cable. |
| Adafruit 16-channel PWM servo driver | The muscle controller. | Uses a chip called the **PCA9685**. |
| 5 hobby servos | The muscles. | Usually cheap plastic-gear "SG90" or "MG996R" style. |
| USB cable | Carries the program from your computer to the Uno. | The Uno has no other way to be programmed. |
| 5 to 6 V power supply | Feeds the servos. | **The Uno is not strong enough to power servos by itself.** See [Power](#power-and-the-one-rule-that-matters-most). |
| Breadboard and jumper wires | Hooks it all together. | |

### Wiring

The Uno and the servo driver board talk to each other over **I2C**, which is just
two data wires plus power:

```
   Arduino Uno                      Adafruit PCA9685 servo driver
  +-------------+                  +---------------------------+
  |          A4 |--- SDA ---------->| SDA                        |
  |          A5 |--- SCL ---------->| SCL                        |
  |         GND |----------------->| GND      (GND is common)   |
  |           5V |----------------->| VCC     (logic power, 5V)  |
  +-------------+                  |                           |
                                     | V+  <--(power supply +)   |
                                     | GND <--(power supply -)   |
  servos plug into the servo         | VM+ -------------------+  |
  header: signals on channels        | G  --------------------+  |
  0-4, red on + and black on G       +---------------------------+

  Power supply:  red/black wires to V+ and GND on the driver board.
  The driver board's GND is already joined to the Uno's GND, so
  the servos and the Uno share a common ground.
```

**SDA is A4 and SCL is A5 on an Uno.** Those two pins are the only ones that
matter for the data connection.

The red wire of a servo goes to `+` on the driver board, the black or brown wire
goes to `G`, and the yellow/orange/white "signal" wire goes into a numbered
channel. The numbers matter, and they are decided in software, not by the
hardware. See [Channel numbers are the wiring](#channel-numbers-are-not-a-detail-they-are-the-wiring).

### Power, and the one rule that matters most

Each servo is a little motor. A servo that stalls can pull over a third of an
ampere. Five of them doing that at once will brown out the Uno, and it will reboot
in the middle of your blink, which is both annoying and eventually harmful.

So: **the servos get their own power supply**, connected to the `V+` and `GND`
terminals of the driver board. The Uno's USB power only runs the brain.

Three rules, in order of how badly you want to avoid breaking them:

1. **The red wire of a servo goes to red, and the black to black.** Reversed
   power on a servo is a fire risk.
2. **The Uno and the power supply must share a ground.** The `GND` pin of the
   driver board is the bridge. If you use a breadboard, plug one power supply's
   ground into that same rail.
3. **The power supply sits outside the breadboard's power rails** if you can.
   Fresh servo power on the rails is fine, but running servos off the `VCC`
   pin of the Uno is not.

---

## How a servo actually works

This is worth ten minutes of reading, because it explains every odd number in
this project.

A hobby servo is a small motor with a gearbox, a position sensor, and a
controller board, all in one plastic case. You tell it an angle, and it turns
its output shaft to that angle and holds it there. It has **no idea how it got
there**, and it cannot tell you where it is.

It learns what you want from a repeating electrical pulse. The pulse repeats
about **50 times a second** (we say 50 Hz), and within each pulse, a longer pulse
means "go further one way" and a shorter pulse means "go further the other way".

```
 one 20-millisecond cycle, called a period:

   1 ms          18 ms of "nothing"
   |<-->|
   |####|____________________________________|

   pulse length = the angle of the servo
   period       = always 20 ms, always
```

Most servos expect a pulse somewhere between about **1 ms and 2 ms**. The numbers
are so small that a bare Arduino pin, which flips every 4 microseconds, is not
precise enough. So instead of counting microseconds, this project uses a chip
that divides one 20 ms cycle into **4096 tiny steps** and counts those.

The driver board in this project is a **PCA9685**, sold by Adafruit as a
"16-channel PWM servo driver". It has 16 outputs. You plug a servo into channel 0
and a servo into channel 4 and they are both driven from the same two I2C wires.
An Uno only has about 6 pins that can do hardware PWM, so a board like this is
how you get past that limit.

### What the numbers mean

If one cycle of 4096 steps lasts 20,000 microseconds, then:

```
one step = 20,000 / 4096 = about 4.88 microseconds
```

The code in this repo calls those steps "pulse counts". So a count of 200 is
roughly `200 x 4.88` = about **977 microseconds** of pulse, and a count of 450 is
about **2,197 microseconds**. Here is the translation table for every number the
code actually uses:

| Count | About (microseconds) | About (milliseconds) | What it is in this project |
|---|---|---|---|
| 150 | 732 | 0.73 | `SERVOMIN`, the lowest the code will ever send |
| 190 | 928 | 0.93 | Right lower lid fully **open** |
| 200 | 977 | 0.98 | Mouth **closed** |
| 240 | 1,172 | 1.17 | Left upper lid **open** |
| 250 | 1,221 | 1.22 | Right lower lid **closed** |
| 280 | 1,367 | 1.37 | Left upper, left lower, and right upper lids **closed** |
| 320 | 1,563 | 1.56 | Right upper lid **open** |
| 330 | 1,611 | 1.61 | Left lower lid **open** |
| 450 | 2,197 | 2.20 | Mouth **open** |
| 600 | 2,930 | 2.93 | `SERVOMAX`, the highest the code will ever send |

These are **approximate**. The Uno's internal clock is not exactly right, so both
sketches call `pwm.setOscillatorFrequency(27000000)` to tell the driver board
that the real clock is 27 MHz rather than the nominal 25 MHz. That is a
calibration trick, and it is why you should trust the servo more than the table.

**The important part is the order of the columns, not the exact values.** The
calibration numbers in the code are numbers *for these specific servos, on these
specific horns, on this specific face*. When you get the face, yours will differ,
and finding your own numbers is a real job. It is the single most satisfying job
in this project.

### The rule you must not break

A servo has a mechanical stop. It is a plastic gear train, and if you command
more travel than the servo can give, the gears grind and strip.

**The code cannot tell a servo at its limit from a servo that is moving freely.**
There is no sensor. So if you send a pulse length that is too long, the servo
sits there buzzing and grinding, and it will break. The only protection is
choosing safe numbers yourself, which is what the calibration in
[`roboface-offline-demo/`](roboface-offline-demo/README.md) is for.

---

## The face's anatomy

Five features. Each one is one servo, on one channel:

| Channel | Feature name | Servo physically does |
|---|---|---|
| 0 | `MOUTH` | Open and close the jaw |
| 1 | `LEFT_EYE_UPPER_LID` | Top of the left eye |
| 2 | `LEFT_EYE_LOWER_LID` | Bottom of the left eye |
| 3 | `RIGHT_EYE_UPPER_LID` | Top of the right eye |
| 4 | `RIGHT_EYE_LOWER_LID` | Bottom of the right eye |

Two of those five numbers are **backwards** compared to the others: the left
upper lid (open 240, closed 280) and the right lower lid (open 190, closed 250).
That is not a bug. It is because whoever built the face clipped those horns on
rotated, so those lids sweep the other way. The code has to describe reality, so
reality is in the file.

That file is `features.h`. It is the one place in this project that decides what
every number means, and it is the first file to open if anything moves the wrong
way.

### Channel numbers are not a detail, they are the wiring

The channel a servo is on is decided in software, but it is decided in **three
places at once**, and they have to agree:

- `features.h` in each directory, which names the channel for each feature.
- The physical wire, which the servo is plugged into.
- The Python scripts, which send channel numbers as raw bytes and have no idea
  what "left eyelid" means.

If you reorder `features.h` and do not move a single wire, one servo suddenly
gets the other's commands. Nothing errors. It just becomes a different face.

And note this sharp edge: **`actions.h` is byte-identical in both directories,
and `positions.h` and `features.h` are near-duplicates, with one copy in each
directory.** (`positions.h` currently differs by an unused `BORED 103` in the
streaming copy, and `features.h` by the streaming sketch's `featureCount` and
`featureMap`.) There is no shared include folder. A calibration fix has to be
made in **both** directories or the two programs will disagree about what 280
means.

---

## Three different kinds of numbers

This trips up everybody, so here it is in one table. The same physical position
is called three different things in this project.

| Where | Units | Mouth closed is | Mouth open is | Who decides |
|---|---|---|---|---|
| Offline demo firmware | raw pulse counts | 200 | 450 | `features.h` |
| Streaming firmware | 0 to 0xff (0 to 255) | 0x00 | 0xff | `features.h` + `Feature.cpp` |
| Python scripts | 0 to 100 (a percentage) | 0 | 100 | the `.py` file |

So "the mouth three quarters open" is `337` to the offline demo, `0xbf` to the
streaming firmware, and `75` to `driver.py`. Three numbers, one position.

The streaming firmware is where the translation happens. A `Feature` object
knows its own minimum and maximum and does this:

```
Feature::mapMe()   maps 0x00 - 0xff onto this feature's own range
```

A useful thing about the way `featureMap` is written: **every row is
`{ CLOSED, OPEN }`**, in that order, for all five features. So sending `0xff`
always means "as open as this feature can go" and `0x00` always means "as closed
as it can go", even for the two features whose numbers run backwards. The
firmware handles the inversion. That is a real design decision, and it is why the
Python scripts never need to know about it.

---

## Libraries you will need to install

Both sketches need Adafruit's servo driver library. Install them from the Arduino
IDE, not by downloading files:

1. Open the Arduino IDE.
2. Menu: **Sketch** → **Include Library** → **Manage Libraries...**
3. In the search box, type the name.
4. Click **Install** on the right one.

| Library name to search for | Needed by | Brings in |
|---|---|---|
| `Adafruit PWM Servo Driver Library` | both | Adafruit's driver for the PCA9685 |
| `TweenDuino` | streaming only | `Tween` and `Timeline`, for smooth motion |
| `ArduinoQueue` | streaming only | the queue library |

`Adafruit PWM Servo Driver Library` may ask to install a couple of dependencies.
Say yes.

---

## Your first day

A path that will not damage anything.

1. Install the Arduino IDE. Connect the Uno over USB. Get the **Blink** example
   from *File → Examples → 01.BuiltinExamples → Blink* to compile and upload. If
   the little LED blinks, you can program a board. Everything from here is the
   same process with more instructions.
2. Wire up the driver board and the servos. With the power supply **off** while
   you do it. Plug in the Uno, not the servos.
3. Open [`roboface-offline-demo/roboface-offline-demo.ino`](roboface-offline-demo/README.md),
   pick **Tools → Board → Arduino Uno**, press Upload.
4. Open *Tools → Serial Monitor*. Set it to **9600** baud and the line ending to
   **Newline**. Type `2` and press enter. You should see a confirmation.
5. Now calibrate, carefully. The
   [offline demo README](roboface-offline-demo/README.md#calibrating-a-servo)
   has the exact procedure. The short version: hold a lid, nudge the number up by
   5 or 10 at a time, stop the moment you feel the servo fight you, back off, and
   write that number down.
6. Write your numbers into `features.h`. **In both directories.**

After that, the interesting part starts.

---

## When something goes wrong

| Symptom | Probably |
|---|---|
| The Serial Monitor shows nothing at all | Wrong baud rate, or wrong port, or the line ending is not set to Newline. The two sketches use different rates: **9600** for the offline demo, **115200** for streaming. |
| The Serial Monitor shows `?` or garbage | Baud rate does not match the sketch. |
| A servo shakes or buzzes continuously | It is at its mechanical stop. Immediately send a number closer to the middle of its range. |
| The board resets when the servos move | Not enough power for the servos. Move them onto the external supply, and check the common ground wire. |
| The servos twitch but the LEDs on the driver board flicker | The driver board lost I2C, usually from brownout. Same fix as above. |
| One servo does nothing | Its signal wire is not in a channel, or the channel number in `features.h` does not match the wire. |
| The face moves the wrong way | Your `features.h` and your wiring disagree. Compare the channel numbers. |
| It uploaded but nothing happened | Check the Serial Monitor first. Both sketches print a startup line. |

---

## Glossary

| Word | Meaning |
|---|---|
| **Sketch** | An Arduino program. The file always ends in `.ino`. |
| **Compile** | Turn the text you wrote into machine instructions the chip can run. This is where errors show up. |
| **Upload** | Send those instructions down the USB cable into the board. |
| **Library** | Prewritten code you add on top of the Arduino's built-in functions, installed by name. |
| **Baud rate** | The speed of a serial connection, in symbols per second. The two ends must agree. |
| **I2C** | Two-wire bus for talking to chips like the PCA9685. Pins A4 and A5 on an Uno. |
| **PWM** | Pulse width modulation. Turning a signal on and off in a pattern to fake a varying level. Here, the pattern's width is the position. |
| **VCC / V+ / VM+** | The driver board's pins. VCC is logic power, V+ is the input power, VM+ is the servo power output. |
| **Counts / pulse counts** | The 4096 steps in one 20 ms cycle. This project's unit for a servo position. |
| **Tween** | Code that smoothly moves a number from one value to another over a period of time, instead of jumping. |

---

## Where to go from here

The goal is animatronics, so here is the honest order in which things become
possible.

**Now.** The offline demo is a full remote control for every servo. Every
expression you can think of, you can do from a keyboard: blinks, a yawn, a laugh,
a slow menacing squint. `actions.h` already has a table of mouth movements and a
timing grid. Adding a new one is a small edit.

**Next.** Add more servos. The driver board has 16 channels and you are using 5.
Eyebrows are the obvious next feature and they change a face more than almost
anything else you can add. Then a jaw that moves forward as well as up and down.
A PCA9685 supports it, and all you have to do is add rows to `features.h` in both
directories. Just be careful: the PWM driver can supply more current than the
board's traces are built for, so power the servos from the supply.

**Then.** Real reactions. `live_audio.py` in the streaming directory is a
prototype that opens the microphone and drives the mouth from how loud the room
is. It is unfinished and awkward, but the idea is sound and the hard part, audio
input, is already solved. Blink on a loud sound. Lean forward when someone
speaks. That is where a face stops being a prop and starts being a character.

**Then.** Remembered movement. The streaming firmware already has a half-built
queue of timed frames and a slot for saved frames, which is a timeline. Finishing
that is how you get "perform a 4-second scared double-take" as a single action.

**Later.** WiFi. An ESP32 is an Arduino-shaped board with WiFi on it, and it can
do everything this Uno does plus talk to a network. Later still, DMX or a serial
bus if you ever need more than one moving prop. Animatronics shows scale, and
scale means coordinated timing across many machines.

**Worth knowing about the field.** Servos are not the only way to do this. A
character animatronic usually runs on **puppets** for repeatable mechanical
motion and **air servos** for fast, strong, precise hits. Hobby servos like
these are great for a face, and are the right first tool. Knowing the difference
early will save you from trying to make a servo do a job it cannot hold.

---

## For whoever maintains this

- Developer and agent notes live in [`AGENTS.md`](AGENTS.md). The module READMEs
  repeat the parts a beginner needs.
- The two sketches share no code at all. They share nothing but the shape of the
  problem, and each directory carries its own copy of `actions.h`,
  `positions.h`, and `features.h`. The streaming sketch includes only
  `features.h` of the three.
- `roboface-streaming/README.md` documents the wire protocol and the remaining
  design notes.
