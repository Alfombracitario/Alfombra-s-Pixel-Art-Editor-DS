#include "timers.h"

u32 timerAccum = 0;
u32 timerMark = 0;
int fps = 0;
int frameCount = 0;
time_t lastTime = 0;

void initFPS() {
    lastTime = time(NULL);
    frameCount = 0;
    fps = 0;
}

void updateFPS() {
    frameCount++;
    time_t now = time(NULL);
    if(now != lastTime) {  // pasó 1 segundo real
        fps = frameCount;
        frameCount = 0;
        lastTime = now;
    }
}
void initTimers() {
    TIMER0_DATA = 0;
    TIMER1_DATA = 0;
    TIMER0_CR = TIMER_ENABLE | TIMER_DIV_1;
    TIMER1_CR = TIMER_ENABLE | TIMER_CASCADE;
}
