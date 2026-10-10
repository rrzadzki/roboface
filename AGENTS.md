# roboface

Firmware for a 5-servo animatronic face (mouth + two upper/lower eyelids) on an
Adafruit PCA9685 16-channel PWM servo driver.

There is **no build system, no test suite, no linter, no CI, and no dependency
manifest**. Two independent Arduino sketches live side by side; they share no code.

## Layout

Each program has a `firmware/` side (Arduino sketch and its headers) and, where
one exists, a `controller/` side (PC-side Python). The Arduino IDE requires a
sketch folder name to match its `.ino`, hence the doubled names.

- `remote/firmware/roboface-remote-firmware/roboface-remote-firmware.ino` —
  range-of-motion / calibration sketch (formerly `roboface-offline-demo`).
  Interactive over serial at **9600 baud**: `Serial.parseInt()` where
  1-16 selects a channel, 101/102 are EYES_CLOSED/EYES_OPEN, 1001/1002 are
  BLINK/TALK, and 150-600 is a raw PWM count. Derived from Adafruit's servo
  demo; `setServoPulse()` and `SERVOMIN/MAX` are unused leftovers. It is driven
  from the Serial Monitor, so there is no `remote/controller/`.
- `streaming/firmware/roboface-streaming-firmware/roboface-streaming-firmware.ino`
  — the live driver. Serial at **115200 baud**. Length-prefixed frames are
  enqueued (streaming) or saved (download mode), then dequeued and tweened into
  place every `time_per_frame` ms (default 1000) while `playing` is true. Also
  holds `Feature.h`/`Feature.cpp` and its own `features.h`/`positions.h`/`actions.h`.
- `streaming/controller/serial_protocol.py` — shared wire helpers (control bytes,
  frame encoding, port selection) used by the host senders.
- `streaming/controller/{driver,playback,live_audio,send_serial}.py` — host-side
  senders. They `import serial_protocol`, so keep them in one directory.

READMEs: root `README.md` (hardware, beginner guide), one per firmware sketch,
and `streaming/controller/README.md` for the Python senders.

## Build / flash

Open the `.ino` in the Arduino IDE and upload with the board you have wired up.
The target is an **Arduino Uno** (`arduino:avr:uno`). These libraries must be
installed via Library Manager; none are vendored: `Adafruit_PWMServoDriver`,
`Tween` by hideakitai (Library Manager auto-installs its dependencies
`ArxContainer`, `ArxSmartPtr`, `ArxTypeTraits`, `Easing`, `PollingTimer`),
`ArduinoQueue`. TweenDuino must be uninstalled: it also ships a `Tween.h` and
can shadow the hideakitai header. Oscillator frequency is set to 27000000 to
match the real internal oscillator, not the nominal 25 MHz.

Tweening uses one shared `Tween::Timeline` in the streaming `.ino`. Each frame
does `timeline.clear()`, then `Feature::setTarget(timeline, ...)` per feature
(`add(pos).init(pos).then(target, ms)`), then `timeline.start()`; `loop()`
calls `timeline.update()` before `Feature::writePWM()`. `Feature.h` defines
`ARX_MAP_DEFAULT_SIZE` (5, must be >= `featureCount`, enforced by a
`static_assert`) and `ARX_VECTOR_DEFAULT_SIZE` (1) before including `Tween.h`
to keep heap use down on the Uno; this heap is not shown in the IDE's
memory report. Adding a feature means raising the map size; chaining more
than one `then()` per sequence means raising the vector size.

## streaming firmware modes

`MODE` is an enum: `STREAMING` (default), `PLAYBACK_DL`, `PLAYBACK`. Wire
control bytes switch between them: `START_DL` (`0xfb`) enters download,
`STOP_DL` (`0xfc`) ends download and enters playback, `PLAY` (`0xfe`)/`STOP`
(`0xfd`) start and stop motion. `savedFrames` holds `SAVED_FRAME_COUNT` (100)
entries and `nextFrameIdx` is bounds-checked before each write.

## Channel numbers are the wire format

Channel index is not a detail of the firmware — host scripts hardcode it.
`features.h` defines the mapping, and `featureMap[i]` must stay row-aligned
with it: `0` MOUTH, `1` LEFT_EYE_UPPER_LID, `2` LEFT_EYE_LOWER_LID,
`3` RIGHT_EYE_UPPER_LID, `4` RIGHT_EYE_LOWER_LID. Inserting or reordering a
feature silently re-points a servo.

## Two different position units — do not mix them

- **Remote** takes and writes *raw PWM counts* straight to
  `pwm.setPWM(channel, 0, count)` (mouth 200-450, lids 190-330).
- **Streaming** takes a normalized 0x00-0xff value; `Feature::mapMe()` maps it
  onto that feature's own `featureMap` min/max. Host scripts speak a third
  unit, 0-100 percent, and convert themselves.

Changing a calibration constant therefore changes what the host scripts must
send, and vice versa.

## Serial protocol (streaming, 115200 8N1)

Length-prefixed frames. A frame is a `0xff` marker, a one-byte pair count `N`,
then `N` `(channel, value)` pairs. Values are full 0-255 bytes.

| Byte | Meaning |
| --- | --- |
| `0xff` | frame start; the next byte is the pair count |
| `N` | number of `(channel, value)` pairs that follow |
| `channel` | feature index (0-4), validated against `featureCount` |
| `value` | 0-255, mapped onto the feature's own min/max |
| `0xfe` | PLAY |
| `0xfd` | STOP |
| `0xfb` | START_DL (enter download mode) |
| `0xfc` | STOP_DL (end download, enter playback) |

Because the count tells the parser exactly how many payload bytes to read,
payload bytes are never mistaken for control bytes (every value is
representable), and a stray `0xff` gives a natural resync point (`N >
featureCount` is treated as desync). The firmware reads one byte at a time with
a small state machine.

## Host scripts

All four are guarded with `if __name__ == '__main__'` and share
`serial_protocol.py` for wire encoding and port selection. The port defaults to
`COM3` and can be overridden with the `ROBOFACE_PORT` env var. Only one script
can hold the port at a time. `live_audio.py` additionally needs `pyaudio` and
`numpy`, and drives the mouth from mic amplitude.

## Duplicated headers

`actions.h` is byte-identical between the two firmware sketches; `positions.h`
differs only by an unused `BORED 103` in the streaming copy, and `features.h`
differs (streaming adds `featureCount` and `featureMap`). A calibration fix must
be applied in both sketch directories — there is no shared include path. The
streaming sketch includes only `features.h` of the three; `positions.h` and
`actions.h` remain in that directory because they are shared with the remote
sketch.

## Known behavioral gaps

- `ArduinoQueue<frame> frameQueue(100)` silently drops frames when the host
  streams faster than one frame per `time_per_frame`.
- Playback auto-stops when its source drains (queue or `savedFrames`), so a
  still-running host must send `PLAY` again to resume.
- Channels a frame omits stay at 0 (fully closed), because `next_frame` is
  zero-initialized when a frame completes.
