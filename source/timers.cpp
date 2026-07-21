#include "timers.h"

#ifdef DEBUG_CPU
u32 timerAccum = 0;
u32 timerMark = 0;
int fps = 0;
int frameCount = 0;
time_t lastTime = 0;
#endif

void initFPS() {
#ifdef DEBUG_CPU
    lastTime = time(NULL);
    frameCount = 0;
    fps = 0;
#endif
}

void __attribute__((section(".itcm"))) updateFPS() {
#ifdef DEBUG_CPU
    frameCount++;
    time_t now = time(NULL);
    if(now != lastTime) {
        fps = frameCount;
        frameCount = 0;
        lastTime = now;
    }
#endif
}

void initTimers() {
#ifdef DEBUG_CPU
    TIMER0_DATA = 0;
    TIMER1_DATA = 0;
    TIMER0_CR = TIMER_ENABLE | TIMER_DIV_1;
    TIMER1_CR = TIMER_ENABLE | TIMER_CASCADE;
#endif
}