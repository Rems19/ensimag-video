#include <stdbool.h>
#include <assert.h>
#include <SDL2/SDL.h>
#include <pthread.h>
#include <SDL_image.h>
#include "ensitheora.h"
#include "synchro.h"
#include "stream_common.h"
#include "ensivorbis.h"

int windowsx = 0;
int windowsy = 0;

int tex_iaff= 0;
int tex_iwri= 0;

bool quit = false;

bool paused = false;

bool fullscreen = false;

static SDL_Window *screen = NULL;
static SDL_Renderer *renderer = NULL;
struct TextureDate texturedate[NBTEX] = {};
SDL_Rect rect = {};

uint32_t last_tick_ms = 0;
uint32_t video_time_ms = 0;
uint32_t last_skip_time_ms = 0;

SDL_Texture *play_icon;
SDL_Texture *pause_icon;
SDL_Texture *skip_icon;
SDL_Texture *current_icon = NULL;
SDL_Rect icon_rect = {};
int icon_change_ms = 0;

SDL_Rect dest_rect = {};

struct streamstate *theorastrstate=NULL;

void window_resize(int width, int height) {
    printf("Window resized to %dx%d\n", width, height);
    float ratio = (float) width / (float) height;
    float video_ratio = (float) windowsx / (float) windowsy;
    if (ratio > video_ratio) {
        dest_rect.w = (int) (video_ratio * (float) height);
        dest_rect.h = height;
        dest_rect.x = (width - dest_rect.w) / 2;
        dest_rect.y = 0;
    } else {
        dest_rect.w = width;
        dest_rect.h = (int) ((float) width / video_ratio);
        dest_rect.x = 0;
        dest_rect.y = (height - dest_rect.h) / 2;
    }

    icon_rect.h = 96;
    icon_rect.w = 96;
    icon_rect.x = (width - icon_rect.w) / 2;
    icon_rect.y = (height - icon_rect.h) / 2;
}

void toggle_fullscreen() {
    if (fullscreen) {
        SDL_SetWindowFullscreen(screen, 0);
        fullscreen = false;
    } else {
        SDL_SetWindowFullscreen(screen, SDL_WINDOW_FULLSCREEN_DESKTOP);
        fullscreen = true;
    }
}

void toggle_pause() {
    paused = !paused;
    if (paused) {
        SDL_PauseAudioDevice(audioid, 1);
        current_icon = pause_icon;
        icon_change_ms = SDL_GetTicks();
    } else {
        SDL_PauseAudioDevice(audioid, 0);
        current_icon = play_icon;
        icon_change_ms = SDL_GetTicks();
    }
}

void *draw2SDL(void *arg) {
    int serial = *((int *) arg);
    struct streamstate *s= NULL;
    SDL_Texture* texture = NULL;

    attendreTailleFenetre();

    int width = windowsx < 1280 ? windowsx : 1280;
    int height = windowsy < 720 ? windowsy : 720;
    window_resize(width, height);

    // create SDL window (if not done) and renderer
    screen = SDL_CreateWindow("Ensimag lecteur ogg/theora/vorbis",
                              SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED,
                              width,
                              height,
                              SDL_WINDOW_RESIZABLE);
    renderer = SDL_CreateRenderer(screen, -1, 0);

    assert(screen);
    assert(renderer);
    // affichage en noir
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);


    // la texture
    texture = SDL_CreateTexture(renderer,
                                SDL_PIXELFORMAT_YV12,
                                SDL_TEXTUREACCESS_STREAMING,
                                windowsx,
                                windowsy);

    play_icon = IMG_LoadTexture(renderer, "../assets/play.png");
    pause_icon = IMG_LoadTexture(renderer, "../assets/pause.png");
    skip_icon = IMG_LoadTexture(renderer, "../assets/skip.png");

    assert(texture);
    // remplir les planes de TextureDate
    for(int i=0; i < NBTEX; i++) {
        texturedate[i].plane[0] = malloc( windowsx * windowsy );
        texturedate[i].plane[1] = malloc( windowsx * windowsy );
        texturedate[i].plane[2] = malloc( windowsx * windowsy );
    }

    signalerFenetreEtTexturePrete();

    /* Protéger l'accès à la hashmap */

    pthread_mutex_lock(&hashmap_mutex);
    HASH_FIND_INT(theorastrstate, &serial, s);
    pthread_mutex_unlock(&hashmap_mutex);

    assert(s->strtype == TYPE_THEORA);

    attendreAudioDevice(); // On se synchronise avec le début de l'audio en prenant en charge l'unpause du device
    SDL_PauseAudioDevice(audioid, 0);

    last_tick_ms = SDL_GetTicks();
    const uint32_t DOUBLE_CLICK_DELAY = 200;
    uint32_t last_click = -1;
    while(!quit && (!(video_fini && audio_fini) || nb_textures > 0)) {

        // Check for single clicks
        if (last_click != -1 && SDL_GetTicks() - last_click > DOUBLE_CLICK_DELAY) {
            toggle_pause();
            last_click = -1;
        }

        // récupérer les évenements de fin
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            // handle your event here
            if (event.type == SDL_QUIT) {
                SDL_AudioStreamClear(audio_stream);
                quit = true;
                video_fini = true;
                audio_fini = true;
                break;
            } else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_SPACE) {
                    toggle_pause();
                } else if (event.key.keysym.sym == SDLK_RIGHT && SDL_GetTicks() - last_skip_time_ms > 50) { // On limite à 20 skip/s
                    uint32_t skip_ms = 5000;
                    pthread_mutex_lock(&texture_rw);
                    uint32_t future_ms = video_time_ms + skip_ms;
                    int before_nb_textures = nb_textures;
                    int before_tex_iaff = tex_iaff;
                    while (texturedate[tex_iaff].timems < future_ms && nb_textures > 0) { // skip frames
                        tex_iaff = (tex_iaff + 1) % NBTEX;
                        nb_textures--;
                    }
                    // TODO: Skip audio better than that :(
                    if (nb_textures == 0 || !audio_skip(skip_ms)) { // On n'avait pas assez de frames ou d'audio d'avance, on annule le skip
                        nb_textures = before_nb_textures;
                        tex_iaff = before_tex_iaff;
                        printf("CAN'T SKIP\n");
                    } else {
                        last_skip_time_ms = SDL_GetTicks();
                        pthread_cond_signal(&texture_w);
                        video_time_ms = future_ms; // Sinon, on adapte video_time_ms
                        current_icon = skip_icon;
                        icon_change_ms = SDL_GetTicks();
                    }
                    pthread_mutex_unlock(&texture_rw);
                } else if (event.key.keysym.sym == SDLK_F11) {
                    toggle_fullscreen();
                }
            } else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                // Double clicks are still not handled correctly by SDL... So we have to take care of it ourselves
                uint32_t now = SDL_GetTicks();
                if (last_click == -1)
                    last_click = now;
                else if (now - last_click <= DOUBLE_CLICK_DELAY) {
                    toggle_fullscreen(); // double click
                    last_click = -1;
                }
            } else if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED) {
                window_resize(event.window.data1, event.window.data2);
            }
        }

        if (quit) {
            break;
        }

        debutConsommerTexture();

        SDL_UpdateYUVTexture(texture, &rect,
                             texturedate[tex_iaff].plane[0],
                             windowsx,
                             texturedate[tex_iaff].plane[1],
                             windowsx,
                             texturedate[tex_iaff].plane[2],
                             windowsx);

        // Copy the texture with the renderer
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, &dest_rect);
        if (current_icon != NULL) {
            uint32_t ms_passed = SDL_GetTicks() - icon_change_ms;
            if (ms_passed >= 1500) {
                SDL_SetTextureAlphaMod(current_icon, 255);
                current_icon = NULL;
            } else {
                int alpha = (int) (255.0 * (1.0 - (ms_passed / 1500.0)));
                alpha = alpha > 255 ? 255 : alpha;
                SDL_SetTextureAlphaMod(current_icon, alpha);
                SDL_RenderCopy(renderer, current_icon, NULL, &icon_rect);
            }
        }
        SDL_RenderPresent(renderer);

        double expire = texturedate[tex_iaff].timems;

        uint32_t now = SDL_GetTicks();
        uint32_t delta = now - last_tick_ms;
//        printf("DELTA: %dms\n", delta);
        last_tick_ms = now;

        if (!paused && video_time_ms <= audio_time_ms - 200) {
            video_time_ms += delta;
            tex_iaff = (tex_iaff + 1) % NBTEX;
            finConsommerTexture();
        }

//        printf("Time: V:%.02fs, A:%.02fs, D:%.02fs\n", video_time_ms / 1000.0, audio_time_ms / 1000.0, (video_time_ms - audio_time_ms) / 1000.0);
        int delaims = (int) (expire - video_time_ms);
        if (delaims > 0.0)
            SDL_Delay(delaims);
    }
    SDL_DestroyTexture(play_icon);
    SDL_DestroyTexture(pause_icon);
    SDL_DestroyTexture(skip_icon);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(screen);
    return 0;
}


void theora2SDL(struct streamstate *s) {
    assert(s->strtype == TYPE_THEORA);

    ogg_int64_t granulpos = -1;
    double framedate; // framedate in seconds
    th_ycbcr_buffer videobuffer;

    int res = th_decode_packetin(s->th_dec.ctx, &s->packet, &granulpos);
    framedate = th_granule_time(s->th_dec.ctx, granulpos);
    if (res == TH_DUPFRAME) // 0 byte duplicated frame
        return;

    assert(res == 0);

    // th_ycbcr_buffer buffer = {};
    static bool once = false;
    if (!once) {
        res = th_decode_ycbcr_out(s->th_dec.ctx, videobuffer);

        // Envoyer la taille de la fenêtre
        envoiTailleFenetre(videobuffer);

        attendreFenetreTexture();

        // copy the buffer
        rect.w = videobuffer[0].width;
        rect.h = videobuffer[0].height;
        // once = true;
    }


    // 1 seul producteur/un seul conso => synchro sur le nb seulement

    debutDeposerTexture();


    if (!once) {
        // for(unsigned int i = 0; i < 3; ++i)
        //    texturedate[tex_iwri].buffer[i] = buffer[i];
        once = true;
    } else
        res = th_decode_ycbcr_out(s->th_dec.ctx, videobuffer);

    // copy data in the current texturedate
    for(int pl = 0; pl < 3; pl++) {
        for(int i = 0; i < videobuffer[pl].height; i++) {
            memmove(
                    texturedate[tex_iwri].plane[pl]+i*windowsx,
                    videobuffer[pl].data+i* videobuffer[pl].stride,
                    videobuffer[pl].width
            );
        }
    }
    texturedate[tex_iwri].timems = framedate * 1000;
    assert(res == 0);
    tex_iwri = (tex_iwri + 1) % NBTEX;

    finDeposerTexture();
}
