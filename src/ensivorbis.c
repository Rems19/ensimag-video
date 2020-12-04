#include <time.h>
#include <assert.h>
#include "ensivideo.h"
#include "ensivorbis.h"
#include "synchro.h"

double audio_decoded_time_ms = 0;
double audio_time_ms = 0;

SDL_AudioDeviceID audioid=0;
SDL_AudioSpec want = {};
SDL_AudioSpec have = {};

SDL_AudioStream *audio_stream = NULL;
int audio_channels = 0;
int audio_rate = 0;

struct streamstate *vorbisstrstate=NULL;

void audio_callback(void *userdata, uint8_t *stream, int len) {
    int available = SDL_AudioStreamAvailable(audio_stream);
    int writable = len > available ? available : len;
    SDL_AudioStreamGet(audio_stream, stream, writable);
    memset(stream + writable, have.silence, len - writable); // Complete with silence if not enough audio (end of program)
    audio_time_ms += 1000.0 * len / sizeof(float) / audio_channels / audio_rate;
}

bool audio_skip(int ms_to_skip) {
    SDL_AudioStreamFlush(audio_stream);
    int available = SDL_AudioStreamAvailable(audio_stream);
    int to_skip = (int) sizeof(float) * ms_to_skip * audio_rate * audio_channels / 1000;
    if (to_skip > available)
        return false;
    float *buffer = malloc(to_skip);
    SDL_AudioStreamGet(audio_stream, buffer, to_skip);
    audio_time_ms += 1000.0 * to_skip / (int) sizeof(float) / audio_channels / audio_rate;
    free(buffer);
    return true;
}

void vorbis2SDL(struct streamstate *s) {
    assert(s->strtype == TYPE_VORBIS);
    if (! audioid) {
        want.freq = s->vo_dec.info.rate;
        want.format = AUDIO_F32;
        want.channels = s->vo_dec.info.channels;
        want.samples = 4096;
        want.callback = audio_callback;

        audioid = SDL_OpenAudioDevice(NULL, false, & want, & have, 0 );

        audio_stream = SDL_NewAudioStream(AUDIO_F32, s->vo_dec.info.channels, s->vo_dec.info.rate, AUDIO_F32, s->vo_dec.info.channels, s->vo_dec.info.rate);
        audio_channels = s->vo_dec.info.channels;
        audio_rate = s->vo_dec.info.rate;
        signalerAudioDevicePret();
    }
    assert(audioid);

    if (vorbis_synthesis( & s->vo_dec.block, &s->packet) == 0) {
        int res = vorbis_synthesis_blockin(& s->vo_dec.dsp,
                                           & s->vo_dec.block);
        assert(res == 0);
    }

    float **pcm = 0;
    int samples = 0;
    while((samples = vorbis_synthesis_pcmout(& s->vo_dec.dsp, & pcm)) > 0) {
        float *tmpbuff = malloc(samples * s->vo_dec.info.channels * sizeof(float));
        for(int sa=0, idx=0; sa < samples; sa++)
            for(int c=0; c < s->vo_dec.info.channels; c++, idx++)
                tmpbuff[idx] = pcm[c][sa];
//        SDL_QueueAudio(audioid, tmpbuff, samples * s->vo_dec.info.channels * sizeof(float));
        SDL_AudioStreamPut(audio_stream, tmpbuff, samples * s->vo_dec.info.channels * sizeof(float));
        free(tmpbuff);
        int res = vorbis_synthesis_read(& s->vo_dec.dsp, samples);
        assert(res == 0);
        audio_decoded_time_ms += 1000 * samples / (double) s->vo_dec.info.rate;
    }

    double ecartms = audio_decoded_time_ms - video_time_ms;
    if (ecartms > 10000.0) { // Si on a plus de 10s d'avance, on se calme
        SDL_Delay((int)(ecartms - 10000.0));
    }
}
