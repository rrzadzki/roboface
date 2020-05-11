

#include <Timeline.h>
#include <Tween.h>
#include <TweenDuino.h>

/*************************************************** 
  Roboface (maybe HELPeR)'s firmware, this sketch
  receives instructions over serial to manipulate the
  face live.
 ****************************************************/

// Uncomment this line for serial debugging
//#define DEBUG

#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <TweenDuino.h>
#include "features.h"
#include "positions.h"
#include "actions.h"

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

#define buffer_length 2
char buffer[buffer_length];
bool move = true;
// our servo # counter
uint8_t servonum = 0;

TweenDuino::Tween *mouthTween;
float mouthPos = MOUTH_CLOSED;

// Some things for messaging
byte CHANNEL_MASK = 0b10000000;
byte channel_indicator = 10;

void setup() {
  Serial.begin(115200);
  Serial.println("8 channel Servo test!");

  mouthTween = TweenDuino::Tween::to(mouthPos, 5000UL, MOUTH_OPEN);

  pwm.begin();
  // In theory the internal oscillator is 25MHz but it really isn't
  // that precise. You can 'calibrate' by tweaking this number till
  // you get the frequency you're expecting!
  pwm.setOscillatorFrequency(27000000);  // The int.osc. is closer to 27MHz  
  pwm.setPWMFreq(SERVO_FREQ);  // Analog servos run at ~50 Hz updates
}

// You can use this function if you'd like to set the pulse length in seconds
// e.g. setServoPulse(0, 0.001) is a ~1 millisecond pulse width. It's not precise!
void setServoPulse(uint8_t n, double pulse) {
  double pulselength;
  
  pulselength = 1000000;   // 1,000,000 us per second
  pulselength /= SERVO_FREQ;   // Analog servos run at ~60 Hz updates
  Serial.print(pulselength); Serial.println(" us per period"); 
  pulselength /= 4096;  // 12 bits of resolution
  Serial.print(pulselength); Serial.println(" us per bit"); 
  pulse *= 1000000;  // convert input seconds to us
  pulse /= pulselength;
  Serial.println(pulse);
  pwm.setPWM(n, 0, pulse);

  
}

void loop() {
  mouthTween->update(millis());
  pwm.setPWM(MOUTH, 0, (int)mouthPos);
  
  /*
  if(Serial.available() > 0) {
    int count = Serial.readBytes(buffer,buffer_length);
    byte channel = -1;
    byte value = -1;
    log("Received ");
    log(count);
    log(" bytes\n");
    for(int i=0; i < count; i++) {
      // Channel indicators start with 0
      // Value indicators start with 1
      if((buffer[i] & CHANNEL_MASK) == 0) {
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
        log("Setting channel ");
        //log(channel);
        log(" to " );
        //log(value);
        log("\n");
        int pos = map(
            value,
            0,
            0x7f,
            featureMap[channel][0],
            featureMap[channel][1]
          );
        pwm.setPWM(channel, 0, pos);
        );
      }
    }
  }
  */
}

void blink() {
  pwm.setPWM(LEFT_EYE_UPPER_LID, 0, LEFT_EYE_UPPER_LID_CLOSED);
  pwm.setPWM(LEFT_EYE_LOWER_LID, 0, LEFT_EYE_LOWER_LID_CLOSED);
  pwm.setPWM(RIGHT_EYE_UPPER_LID, 0, RIGHT_EYE_UPPER_LID_CLOSED);
  pwm.setPWM(RIGHT_EYE_LOWER_LID, 0, RIGHT_EYE_LOWER_LID_CLOSED);
  delay(100);
  pwm.setPWM(LEFT_EYE_UPPER_LID, 0, LEFT_EYE_UPPER_LID_OPEN);
  pwm.setPWM(LEFT_EYE_LOWER_LID, 0, LEFT_EYE_LOWER_LID_OPEN);
  pwm.setPWM(RIGHT_EYE_UPPER_LID, 0, RIGHT_EYE_UPPER_LID_OPEN);
  pwm.setPWM(RIGHT_EYE_LOWER_LID, 0, RIGHT_EYE_LOWER_LID_OPEN);
}

void talk() {
  // Do it a few times because this is short and dulllll
  for(int j=0; j<5; j++) {
    for(int i=0; i<5; i++) {
      pwm.setPWM(
        MOUTH,
        0,
        map(
          talk_map[i][0], // value
          0,              // in min
          100,            // in max
          MOUTH_CLOSED,   // out min
          MOUTH_OPEN      // out max
        )
      );
      delay(talk_map[i][1]);
    }
  }
  // Close that mouth when done talking - don't do this automatically!
  pwm.setPWM(MOUTH, 0, MOUTH_CLOSED);
}

inline void log(String s) {
  #ifdef DEBUG
  Serial.print(s);
  #endif
}
