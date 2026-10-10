/**
 * Describes the components of a single PWM feature.
 */

#ifndef ROBO_FEATURE
#define ROBO_FEATURE

// Tween (hideakitai) stores its sequences in ArxContainer's fixed-size
// containers on AVR. The defaults (16 slots each) cost ~1 KB of heap, which the
// Uno cannot spare, so size them to what this sketch actually uses. These must
// be defined before the first #include <Tween.h> in every translation unit;
// Feature.h is the only place that includes it.
#define ARX_MAP_DEFAULT_SIZE 5    // one Sequence per feature; must be >= featureCount
#define ARX_VECTOR_DEFAULT_SIZE 1 // each Sequence holds exactly one then()

#include <Tween.h>
#include <Adafruit_PWMServoDriver.h>

class Feature {
public:
  Feature();
  Feature(Adafruit_PWMServoDriver *pwmD, int chan, int minimum, int maximum);
  void setAll(Adafruit_PWMServoDriver *pwmD, int chan, int minimum, int maximum);
  // Adds a tween of this feature to a 0x00-0xff position over ms milliseconds.
  // The caller owns the timeline: clear() it before, start() it after.
  void setTarget(Tween::Timeline &timeline, int to, long ms);
  void writePWM();

private:
  int channel;
  int minPos;
  int maxPos;
  float pos;
  Adafruit_PWMServoDriver *pwm;
  int mapMe(int to);
};

#endif
