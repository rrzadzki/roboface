#include <ArduinoQueue.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include "features.h"
#include "Feature.h"

/*************************************************** 
  Roboface firmware. Receives length-prefixed frames
  over serial and tweens the face between them.
 ****************************************************/

// Uncomment this line for serial debugging
//#define DEBUG

#define SERVO_FREQ 50 // Analog servos run at ~50 Hz updates

// Wire control bytes.
#define FRAME_START 0xff
#define PLAY        0xfe
#define STOP        0xfd
#define STOP_DL     0xfc
#define START_DL    0xfb

// Operating modes.
enum Mode { STREAMING, PLAYBACK_DL, PLAYBACK };
Mode MODE = STREAMING;

// called this way, it uses the default address 0x40
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();

// One frame: a position per feature, in feature order.
struct frame { unsigned char positions[featureCount]; };

// All the face's features. The order of this array MUST match features.h.
Feature features[featureCount];

// The time used to tween from one frame to the next.
unsigned long time_per_frame = 1000;
unsigned long last_frame_start = 0;

// Streaming frame queue.
unsigned int frameQueueSize = 100;
ArduinoQueue<frame> frameQueue(frameQueueSize);
frame next_frame;

// Saved frames for playback, plus a download cursor and play cursor.
#define SAVED_FRAME_COUNT 100
frame savedFrames[SAVED_FRAME_COUNT];
unsigned int nextFrameIdx = 0;
unsigned int playbackIdx = 0;

bool playing = false;

// Serial frame parser state.
enum ParseState { P_IDLE, P_COUNT, P_PAYLOAD };
ParseState parseState = P_IDLE;
unsigned int payloadRemaining = 0;
bool haveChannel = false;
unsigned char pendingChannel = 0;

void log(String s) {
  (void)s;
#ifdef DEBUG
  Serial.print(s);
#endif
}

void onFrameComplete() {
  if (MODE == STREAMING) {
    frameQueue.enqueue(next_frame);
  } else if (MODE == PLAYBACK_DL) {
    if (nextFrameIdx < SAVED_FRAME_COUNT) {
      savedFrames[nextFrameIdx++] = next_frame;
    }
  }
  next_frame = frame();
}

void readSerial() {
  while (Serial.available() > 0) {
    int b = Serial.read();

    switch (parseState) {
      case P_IDLE:
        switch (b) {
          case FRAME_START:
            parseState = P_COUNT;
            break;
          case PLAY:
            log("PLAY\n");
            if (MODE == PLAYBACK) {
              playbackIdx = 0;
            }
            playing = true;
            last_frame_start = millis();
            break;
          case STOP:
            log("STOP\n");
            playing = false;
            break;
          case START_DL:
            log("START_DL\n");
            if (MODE == STREAMING || MODE == PLAYBACK) {
              MODE = PLAYBACK_DL;
              nextFrameIdx = 0;
              next_frame = frame();
            }
            break;
          case STOP_DL:
            log("STOP_DL\n");
            if (MODE == PLAYBACK_DL) {
              MODE = PLAYBACK;
              playbackIdx = 0;
            }
            break;
          default:
            break; // discard unknown byte
        }
        break;

      case P_COUNT:
        if (b > featureCount) {
          // Unreasonable pair count: we are out of sync. Resync.
          parseState = P_IDLE;
          break;
        }
        payloadRemaining = (unsigned int)b * 2;
        haveChannel = false;
        parseState = (payloadRemaining == 0) ? P_IDLE : P_PAYLOAD;
        break;

      case P_PAYLOAD:
        if (!haveChannel) {
          pendingChannel = (unsigned char)b;
          haveChannel = true;
        } else {
          if (pendingChannel < featureCount) {
            next_frame.positions[pendingChannel] = (unsigned char)b;
          }
          haveChannel = false;
        }
        payloadRemaining--;
        if (payloadRemaining == 0) {
          onFrameComplete();
          parseState = P_IDLE;
        }
        break;
    }
  }
}

void playFrames() {
  if (!playing) {
    return;
  }

  if (last_frame_start + time_per_frame <= millis()) {
    frame f;
    bool haveFrame = false;

    if (MODE == STREAMING) {
      if (frameQueue.itemCount() > 0) {
        f = frameQueue.dequeue();
        haveFrame = true;
      }
    } else if (MODE == PLAYBACK) {
      if (playbackIdx < nextFrameIdx) {
        f = savedFrames[playbackIdx++];
        haveFrame = true;
      }
    }

    if (haveFrame) {
      for (int i = 0; i < featureCount; i++) {
        features[i].setTarget(f.positions[i], time_per_frame);
      }
      last_frame_start = millis();
    } else {
      playing = false;
    }
  }

  for (int i = 0; i < featureCount; i++) {
    features[i].updatePWM();
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Roboface ready");

  pwm.begin();
  // In theory the internal oscillator is 25MHz but it really isn't
  // that precise. The real internal oscillator is closer to 27MHz.
  pwm.setOscillatorFrequency(27000000);
  pwm.setPWMFreq(SERVO_FREQ);  // Analog servos run at ~50 Hz updates

  for (int i = 0; i < featureCount; i++) {
    features[i].setAll(&pwm, i, featureMap[i][0], featureMap[i][1]);
    features[i].setTarget(0, time_per_frame);
  }
}

void loop() {
  readSerial();
  playFrames();
}
