#import "Feature.h"

Feature::Feature() {
}

Feature::Feature(Adafruit_PWMServoDriver *pwmD, int chan, int minimum, int maximum) {
  channel = chan;
  minPos = minimum;
  maxPos = maximum;
  pos = minPos;
  tween = TweenDuino::Tween::to(pos, 1000, maxPos);
  pwm = pwmD;
}

Feature::~Feature() {
  // TODO
}


void Feature::setAll(Adafruit_PWMServoDriver *pwmD, int chan, int minimum, int maximum) {
  channel = chan;
  minPos = minimum;
  maxPos = maximum;
  pos = mapMe(minPos);
  tween = TweenDuino::Tween::to(pos, 1000, maxPos);
  pwm = pwmD;
}

void Feature::setPos(int p) {
  if(p >= minPos && p <= maxPos) {
    pos = mapMe(p);
    pwm->setPWM(channel,0,pos);
  }
}

void Feature::setTarget(int to, long ms) {
  free(tween);
  // "to" is a value 0 through 0x7f; map it
  // onto the range of this feature.
  
  tween = TweenDuino::Tween::to(
    pos,
    ms,
    mapMe(to)
  );
}

void Feature::updatePWM() {
  tween->update(millis());
  pwm->setPWM(channel, 0, pos);
}

int Feature::mapMe(int to) {
  return map(
      to,
      0,
      0x7f,
      minPos,
      maxPos
    );
}
