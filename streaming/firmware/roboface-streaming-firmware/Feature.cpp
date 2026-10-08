#include "Feature.h"

Feature::Feature() {
  channel = 0;
  minPos = 0;
  maxPos = 0;
  pos = 0;
  tween = nullptr;
  pwm = nullptr;
}

Feature::Feature(Adafruit_PWMServoDriver *pwmD, int chan, int minimum, int maximum)
  : channel(0), minPos(0), maxPos(0), pos(0), tween(nullptr), pwm(nullptr) {
  setAll(pwmD, chan, minimum, maximum);
}

Feature::~Feature() {
  if (tween) {
    delete tween;
  }
}

void Feature::setAll(Adafruit_PWMServoDriver *pwmD, int chan, int minimum, int maximum) {
  channel = chan;
  minPos = minimum;
  maxPos = maximum;
  pos = minimum;
  if (tween) {
    delete tween;
    tween = nullptr;
  }
  pwm = pwmD;
}

void Feature::setTarget(int to, long ms) {
  // "to" is a value 0 through 255; map it onto this feature's range.
  if (tween) {
    delete tween;
  }
  tween = TweenDuino::Tween::to(pos, ms, mapMe(to));
}

void Feature::updatePWM() {
  if (!tween) {
    return;
  }
  tween->update(millis());
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
