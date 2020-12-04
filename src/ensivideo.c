#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <SDL2/SDL.h>
#include <pthread.h>

#include "stream_common.h"
#include "oggstream.h"
#include "synchro.h"


int main(int argc, char *argv[]) {
    int res;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s FILE", argv[0]);
        exit(EXIT_FAILURE);
    }
    assert(argc == 2);


    // Initialisation de la SDL
    res = SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_EVENTS);
    atexit(SDL_Quit);
    assert(res == 0);

    // Initialize semaphores
//    sem_init(&window_size_sem, 0, 0);
//    sem_init(&texture_sem, 0, 0);

    // start the two stream readers
    pthread_create(&audio_reader_pid, NULL, vorbisStreamReader, argv[1]);
    pthread_create(&video_reader_pid, NULL, theoraStreamReader, argv[1]);

    // wait audio thread
    pthread_join(audio_reader_pid, &status);

    // 1 seconde de garde pour le son,
    sleep(1);

    // tuer les deux threads videos si ils sont bloqués
    pthread_cancel(video_reader_pid);
//    pthread_cancel(video_display_pid); // Je décide de ne pas kill le display car je prends de l'avance sur le décodage, c'est donc normal que le display n'ait pas fini

    // attendre les 2 threads videos
    pthread_join(video_reader_pid, &status);
    pthread_join(video_display_pid, &status);

    exit(EXIT_SUCCESS);    
}
