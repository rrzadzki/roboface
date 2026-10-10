#include "Feature.h"

Feature::Feature() {
  channel = 0;
  minPos = 0;
  maxPos = 0;
  pos = 0;
  pwm = nullptr;
}

Feature::Feature(Adafruit_PWMServoDriver *pwmD, int chan, int minimum, int maximum)
  : channel(0), minPos(0), maxPos(0), pos(0), pwm(nullptr) {
  setAll(pwmD, chan, minimum, maximum);
}

void Feature::setAll(Adafruit_PWMServoDriver *pwmD, int chan, int minimum, int maximum) {
  channel = chan;
  minPos = minimum;
  maxPos = maximum;
  pos = minimum;
  pwm = pwmD;
}

void Feature::setTarget(Tween::Timeline &timeline, int to, long ms) {
  // "to" is a value 0 through 255; map it onto this feature's range.
  // Start from wherever the feature is now.
  timeline.add(pos)
    .init(pos)
    .then((float)mapMe(to), ms);
}

void Feature::writePWM() {
  if (!pwm) {
    return;
  }
  pwm->setPWM(channel, 0, (int)pos);
}

int Feature::mapMe(int to) {
  return map(
      to,
      0,
      255,
      minPos,
      maxPos
    );
}
