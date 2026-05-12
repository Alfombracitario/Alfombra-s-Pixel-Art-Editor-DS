#ifndef TIMERS_H
#define TIMERS_H

#include <nds.h>
#include <time.h>

//#define DEBUG_CPU

#ifdef DEBUG_CPU
extern u32 timerAccum;
extern u32 timerMark;
extern int fps;
extern int frameCount;
extern time_t lastTime;
#endif

void initFPS();
void updateFPS();
void initTimers();

#ifdef DEBUG_CPU
inline u32 getTimeTicks() {
    u32 lo = TIMER0_DATA;
    u32 hi = TIMER1_DATA;
    return (hi << 16) | lo;
}

inline void timerContinue() { timerMark = getTimeTicks(); }
inline void timerStop()     { timerAccum += getTimeTicks() - timerMark; }
inline u32  timerRead()     { return timerAccum; }
inline void timerReset()    { timerAccum = 0; }
#else
inline u32  getTimeTicks()  { return 0; }
inline void timerContinue() {}
inline void timerStop()     {}
inline u32  timerRead()     { return 0; }
inline void timerReset()    {}
#endif

#endif