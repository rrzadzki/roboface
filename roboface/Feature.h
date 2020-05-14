/**
 * Describes the components of a single PWM feature.
 */

#ifndef ROBO_FEATURE
#define ROBO_FEATURE

#include <TweenDuino.h>
#include <Adafruit_PWMServoDriver.h>

class Feature {
public:
  Feature();
  Feature(Adafruit_PWMServoDriver *pwmD, int chan, int minimum, int maximum);
  ~Feature();
  void setAll(Adafruit_PWMServoDriver *pwmD, int chan, int minimum, int maximum);
  void setPos(int pos);
  void setTarget(int to, long ms);
  void updatePWM();

private:
  int channel;
  int minPos;
  int maxPos;
  float pos;
  TweenDuino::Tween *tween;
  Adafruit_PWMServoDriver *pwm;
  int mapMe(int to);
};

#endif
