# roboface

Firmware for a 5-servo animatronic face (mouth + two upper/lower eyelids) on an
Adafruit PCA9685 16-channel PWM servo driver.

There is **no build system, no test suite, no linter, no CI, and no dependency
manifest**. Two independent Arduino sketches live side by side; they share no code.

## Layout

- `roboface-offline-demo/roboface-setup.ino` — range-of-motion / calibration
  sketch. Interactive over serial at **9600 baud**: `Serial.parseInt()` where
  1-16 selects a channel, 101/102 are EYES_CLOSED/EYES_OPEN, 1001/1002 are
  BLINK/TALK, and 150-600 is a raw PWM count. Derived from Adafruit's servo
  demo; `setServoPulse()` and `SERVOMIN/MAX` are unused leftovers.
- `roboface-streaming/roboface.ino` — the live driver. Serial at **115200 baud**,
  commands accumulate into `next_frame`, get enqueued as a `frame` on the
  end-of-frame byte, and are dequeued and tweened into place every
  `time_per_frame` ms (default 1000) while `playing` is true.
- `roboface-streaming/{driver,playback,live_audio,send_serial}.py` — host-side
  senders, not a library.

## Build / flash

Open the `.ino` in the Arduino IDE and upload with the board you have wired up.
The board/FQBN is **not recorded anywhere in this repo** — ask before assuming.
These libraries must be installed via Library Manager; none are vendored:
`Adafruit_PWMServoDriver`, `TweenDuino` (pulls in `Tween`/`Timeline`),
`ArduinoQueue`. Oscillator frequency is set to 27000000 to match the real
internal oscillator, not the nominal 25 MHz.

## roboface-streaming does not compile as committed

Last commit is "WIP on more distinct playback modes"; the refactor was left
half-finished. Expect to fix these before you can upload or judge a change:

- `MODE` is used at roboface.ino:120/163/203 but never defined, nor are
  `STREAMING`, `PLAYBACK`, `PLAYBACK_DL`, `STOP_DL`, or `nextFrameIdx`.
- roboface.ino:165 has unbalanced parens: `if((buffer[i] == STOP_DL) {`.
- roboface.ino:166 assigns with `==` instead of `=` (a MODE transition).
- The `PLAYBACK`/`PLAYBACK_DL` branches are mutually exclusive `else if`s on a
  constant-ish `MODE`, and the download path writes `savedFrames[nextFrameIdx]`
  with no bound on `nextFrameIdx` against `savedFrames[100]`.

**The offline demo is the only sketch that builds.** Use it to sanity-check
hardware and calibration changes.

## Channel numbers are the wire format

Channel index is not a detail of the firmware — host scripts hardcode it.
`features.h` defines the mapping, and `featureMap[i]` must stay row-aligned
with it: `0` MOUTH, `1` LEFT_EYE_UPPER_LID, `2` LEFT_EYE_LOWER_LID,
`3` RIGHT_EYE_UPPER_LID, `4` RIGHT_EYE_LOWER_LID. Inserting or reordering a
feature silently re-points a servo.

## Two different position units — do not mix them

- **Offline demo** takes and writes *raw PWM counts* straight to
  `pwm.setPWM(channel, 0, count)` (mouth 200-450, lids 190-330).
- **Streaming** takes a normalized 0x00-0x7f value; `Feature::mapMe()` maps it
  onto that feature's own `featureMap` min/max. Host scripts speak a third
  unit, 0-100 percent, and convert themselves.

Changing a calibration constant therefore changes what the host scripts must
send, and vice versa.

## Serial protocol (streaming, 115200 8N1)

One byte, high bit selects the meaning — the high bit is stripped off the
payload:

| Byte | Meaning |
| --- | --- |
| `0x00`-`0x7f` | channel index |
| `0x80`-`0xff` | value for the most recent channel |
| `0xff` | end of frame; enqueue the accumulated frame |
| `0xfe` | PLAY |
| `0xfd` | STOP |

Collision to be aware of: the three control bytes are indistinguishable from
value bytes, so values `0x7d`-`0x7f` (percent 98-100) can never be
transmitted. `driver.py` maps percent onto `0x00`-`0x7e`, which puts percent
100 exactly on the `PLAY` byte. Framing is also fragile because the firmware
reads a fixed `buffer_length` of 2 bytes per `readBytes` with no resync, so a
short read desynchronizes channel/value pairing.

## Host scripts

All four open `COM4` at 115200 **at import time**, with no
`if __name__ == '__main__'` guard — `driver.py` drops into a prompt loop the
moment it is imported. Only one can hold the port. `COM4` is hardcoded in each
file; there is no env var or CLI flag. `live_audio.py` additionally needs
`pyaudio` and `numpy`, and drives the mouth from mic amplitude.

## Duplicated headers

`actions.h` and `positions.h` are byte-identical between the two directories;
`features.h` differs (streaming adds `featureCount` and `featureMap`). A
calibration fix must be applied in both directories — there is no shared
include path. `BORED` in `roboface-streaming/positions.h` is unused.

## Known behavioral gaps

- `Feature::updatePWM()` writes `Feature::pos`, but the tween's current value is
  never read back into it, so `setTarget()` currently has no visible effect.
  `setAll()` also double-maps its minimum (`pos = mapMe(minPos)`).
- `ArduinoQueue<frame> frameQueue(100)` silently drops frames when the host
  streams faster than one frame per `time_per_frame`.
- Playback auto-stops when the queue drains, so a still-running host must send
  `PLAY` again to resume.
