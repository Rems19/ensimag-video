#ifndef SYNCHRO_H
#define SYNCHRO_H

#include <stdbool.h>
#include "ensitheora.h"

/* Les extern des variables pour la synchro ici */
extern void *status;
extern pthread_t audio_reader_pid;
extern pthread_t video_reader_pid;
extern pthread_t video_display_pid;

extern pthread_mutex_t hashmap_mutex;

extern pthread_mutex_t taille_mutex;
extern pthread_cond_t cond_taille;
extern bool taille_ready;

extern pthread_mutex_t fenetre_mutex;
extern pthread_cond_t cond_fenetre;
extern bool fenetre_ready;

extern pthread_mutex_t audio_device_mutex;
extern pthread_cond_t cond_audio_device;
extern bool audio_device_ready;

//extern sem_t window_size_sem;
//extern sem_t texture_sem;

extern pthread_mutex_t texture_rw;
extern pthread_cond_t texture_r;
extern pthread_cond_t texture_w;
extern int nb_textures;

/* Fonctions de synchro à implanter */

void envoiTailleFenetre(th_ycbcr_buffer buffer);
void attendreTailleFenetre();

void attendreFenetreTexture();
void signalerFenetreEtTexturePrete();

void attendreAudioDevice();
void signalerAudioDevicePret();

void debutConsommerTexture();
void finConsommerTexture();

void debutDeposerTexture();
void finDeposerTexture();

#endif
