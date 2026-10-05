/*************************************************** 
  Based on Adafruit's demo of the PWM servo board,
  this is the setup sketch for playing around with
  ol' roboface (HELPeR?). It's handy for trying out
  basic stuff but not ideal for any presentation with
  the face.
 ****************************************************/

#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
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

bool move = true;
// our servo # counter
uint8_t servonum = 0;

void setup() {
  Serial.begin(9600);
  Serial.println("8 channel Servo test!");

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
  if(Serial.available() > 0) {
    long input = Serial.parseInt();
    // A low number is a motor channel
    if(input <= 16 && input > 0) {
      servonum = input - 1;
      Serial.print("Set selected motor channel to ");
      Serial.println(servonum);
    } else if (input > 100 && input < 150) {
      // Preset positions for all features. See positions.h
      if(input == EYES_OPEN) {
        pwm.setPWM(LEFT_EYE_UPPER_LID, 0, LEFT_EYE_UPPER_LID_OPEN);
        pwm.setPWM(LEFT_EYE_LOWER_LID, 0, LEFT_EYE_LOWER_LID_OPEN);
        pwm.setPWM(RIGHT_EYE_UPPER_LID, 0, RIGHT_EYE_UPPER_LID_OPEN);
        pwm.setPWM(RIGHT_EYE_LOWER_LID, 0, RIGHT_EYE_LOWER_LID_OPEN);
      } else if(input == EYES_CLOSED) {
        pwm.setPWM(LEFT_EYE_UPPER_LID, 0, LEFT_EYE_UPPER_LID_CLOSED);
        pwm.setPWM(LEFT_EYE_LOWER_LID, 0, LEFT_EYE_LOWER_LID_CLOSED);
        pwm.setPWM(RIGHT_EYE_UPPER_LID, 0, RIGHT_EYE_UPPER_LID_CLOSED);
        pwm.setPWM(RIGHT_EYE_LOWER_LID, 0, RIGHT_EYE_LOWER_LID_CLOSED);
      }
    } else if (input >= SERVOMIN && input <= SERVOMAX) {
      pwm.setPWM(servonum, 0, input);
      Serial.print("Set position of motor ");
      Serial.print(servonum);
      Serial.print(" to ");
      Serial.println(input);
    } else if (input > 1000 && input < 1100) {
      if(input == BLINK) {
        blink();
      } else if(input == TALK) {
        talk();
      }
    }
  }
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
          talk_map[i][0],
          0,
          100,
          MOUTH_CLOSED,
          MOUTH_OPEN
        )
      );
      delay(talk_map[i][1]);
    }
  }
  // Close that mouth when done talking - don't do this automatically!
  pwm.setPWM(MOUTH, 0, MOUTH_CLOSED);
}
