#ifndef ENSIVORBIS_H
#define ENSIVORBIS_H

#include <SDL2/SDL.h>
#include <stdbool.h>

extern SDL_AudioStream *audio_stream;
extern double audio_time_ms;

extern SDL_AudioDeviceID audioid;
extern struct streamstate *vorbisstrstate;
extern bool audio_skip(int ms_to_skip);
extern void vorbis2SDL(struct streamstate *s);

#endif
