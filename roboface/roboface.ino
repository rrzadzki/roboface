#include <ArduinoQueue.h>
#include <Timeline.h>
#include <Tween.h>
#include <TweenDuino.h>

/*************************************************** 
  Roboface (maybe HELPeR)'s firmware, this sketch
  receives instructions over serial to manipulate the
  face live.
 ****************************************************/

// Uncomment this line for serial debugging
#define DEBUG

#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <TweenDuino.h>
#include "features.h"
#include "positions.h"
#include "actions.h"
#include "Feature.h"

// called this way, it uses the default address 0x40
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();
// you can also call it with a different address you want
//Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x41);
// you can also call it with a different address and I2C interface
//Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40, Wire);

// Depending on your servo make, the pulse width min and max may vary, you 
// want these to be as small/large as possible without hitting the hard stop
// for max range. You'll have to tweak them as necessary to match the servos you
// have!
#define SERVOMIN  150 // This is the 'minimum' pulse length count (out of 4096)
#define SERVOMAX  600 // This is the 'maximum' pulse length count (out of 4096)
#define USMIN  600 // This is the rounded 'minimum' microsecond length based on the minimum pulse of 150
#define USMAX  2400 // This is the rounded 'maximum' microsecond length based on the maximum pulse of 600
#define SERVO_FREQ 50 // Analog servos run at ~50 Hz updates
#define TIMELINE_SIZE 10 // for TweenDuino

#define FRAME_STOP_INDICATOR 0xff
#define PLAY 0xfe
#define STOP 0xfd

/*
 * For the serial input buffer
 */
#define buffer_length 2
char buffer[buffer_length];

/*
 * All the face's features. The order of this array MUST
 * match the channel allocation for each feature.
 */
Feature features[featureCount];

/*
 * Definitions for identifying packet types.
 */
const byte CHANNEL_MASK = 0b10000000;
const byte channel_indicator = 10;

/*
 * The time used to tween from one frame to the next.
 */
unsigned long time_per_frame = 1000;
unsigned long last_frame_start = 0;

/*
 * For streaming frames, a queue.
 */
// One frame:
typedef struct frame { unsigned char positions[featureCount]; };

// All frames:
unsigned int frameQueueSize = 100; // todo tune this to maximum
ArduinoQueue<frame> frameQueue(frameQueueSize);
frame next_frame;

/*
 * For saving frames and playing them back, an array.
 */
frame savedFrames[100];


/*** Memory test ***/
#ifdef __arm__
// should use uinstd.h to define sbrk but Due causes a conflict
extern "C" char* sbrk(int incr);
#else  // __ARM__
extern char *__brkval;
#endif  // __arm__

int freeMemory() {
  char top;
#ifdef __arm__
  return &top - reinterpret_cast<char*>(sbrk(0));
#elif defined(CORE_TEENSY) || (ARDUINO > 103 && ARDUINO != 151)
  return &top - __brkval;
#else  // __arm__
  return __brkval ? &top - __brkval : &top - __malloc_heap_start;
#endif  // __arm__
}
/*** End of memory test ***/

bool playing = false;

void setup() {
  Serial.begin(115200);
  Serial.println("8 channel Servo test!");

  pwm.begin();
  // In theory the internal oscillator is 25MHz but it really isn't
  // that precise. You can 'calibrate' by tweaking this number till
  // you get the frequency you're expecting!
  pwm.setOscillatorFrequency(27000000);  // The int.osc. is closer to 27MHz  
  pwm.setPWMFreq(SERVO_FREQ);  // Analog servos run at ~50 Hz updates

  //features = malloc(featureCount * sizeof(Feature));
  for(int i = 0; i < featureCount; i++) {
    features[i].setAll(
      &pwm,
      i,
      featureMap[i][0],
      featureMap[i][1]
    );
    features[i].setPos(0x7f);
    features[i].setTarget(0, time_per_frame);
  }
}

void loop() {
  if(Serial.available() > 0) {
    int count = Serial.readBytes(buffer,buffer_length);
    byte channel = -1;
    byte value = -1;
    log("Received ");
    log(count);
    log(" bytes\n");
    for(int i=0; i < count; i++) {
      if((unsigned char)buffer[i] == FRAME_STOP_INDICATOR) {
        /* Add the existing next_frame to the queue,
        *  set an empty next_frame.
        */
        log("EOF");
        frameQueue.enqueue(next_frame);
        next_frame = frame();
      } else if ((unsigned char)buffer[i] == PLAY) {
        log("PLAY");
        playing = true;
        last_frame_start = millis();
      } else if ((unsigned char)buffer[i] == STOP) {
        log("STOP");
        playing = false;
      }
      // Channel indicators start with 0
      // Value indicators start with 1
      else if((buffer[i] & CHANNEL_MASK) == 0) {
        // This indicates a channel. Mask out the flag bit to find which.
        channel = (buffer[i] & ~CHANNEL_MASK);
        log("C = ");
        //log(channel,BIN);
        log("\n");
      } else {
        // This is a value set. Mask out the flag to get the value.
        value = (buffer[i] & ~CHANNEL_MASK);
        log("V = ");
        //log(value);
        log("\n");
      }

      if(value > -1 && value <= 0x7f && channel > -1) {
        /*
         * Enqueue this instruction
         */
        log("Setting channel ");
        //log(channel);
        log(" to " );
        //log(value);
        log("\n");
        next_frame.positions[channel] = value;
      }
    }
  }

  /*
   * Play frames, if in the playing state and frames
   * are available to play.
   */
  if(playing) {
    // If there are frames available and we're due for a new one...
    if(last_frame_start + time_per_frame <= millis()) {
      if(frameQueue.item_count() > 0) {
        // Get the next frame, set all features to its target positions.
        frame f = frameQueue.dequeue();
        for(int i = 0; i < featureCount; i++) {
          features[i].setTarget(f.positions[i],time_per_frame);
        }
        last_frame_start = millis();
      } else {
        playing = false;
      }
    }
    for(int i = 0; i < featureCount; i++) {
      features[i].updatePWM();
    }
  }
}


inline void log(String s) {
  #ifdef DEBUG
  Serial.print(s);
  #endif
}
