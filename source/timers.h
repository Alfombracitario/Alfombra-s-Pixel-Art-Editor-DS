#ifndef TIMERS_H
#define TIMERS_H

#include <nds.h>
#include <time.h>

extern u32 timerAccum;
extern u32 timerMark;
extern int fps;
extern int frameCount;
extern time_t lastTime;

void initFPS();
void updateFPS();
void initTimers();

inline u32 getTimeTicks() {
    u32 lo = TIMER0_DATA;
    u32 hi = TIMER1_DATA;
    return (hi << 16) | lo;
}

inline void timerContinue() { timerMark = getTimeTicks(); }
inline void timerStop()     { timerAccum += getTimeTicks() - timerMark; }
inline u32  timerRead()     { return timerAccum; }
inline void timerReset()    { timerAccum = 0; }

#endif