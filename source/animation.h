#ifndef ANIMATION_H
#define ANIMATION_H
#include <nds.h>

#define APP_PATH "/_nds/apixds/"
#define CACHE_PATH APP_PATH "cache/"
#define ANIM_TEMP CACHE_PATH "animation.temp"
#define ANIM_TEMP_NEW CACHE_PATH "animation_new.temp"

void loadAnimFrame(u16 *surface);
void saveAnimFrame();
void nextAnimFrame();
void prevAnimFrame();
void deleteAnimFrame();
void insertAnimFrame();
void playAnimation();

struct Animation
{
    u16 frames : 15 = 0;
    u16 isPlaying : 1 = 0;
    u16 pos = 0;
    u8 speed = 2; // en frames
};

extern Animation animation;

#endif