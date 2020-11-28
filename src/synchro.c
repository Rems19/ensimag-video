#include <pthread.h>
#include "synchro.h"


extern bool fini;

/* les variables pour la synchro, ici */
void *status;
pthread_t audio_reader_pid;
pthread_t video_reader_pid;
pthread_t video_display_pid;

pthread_mutex_t hashmap_mutex;

pthread_mutex_t taille_mutex;
pthread_cond_t cond_taille;
bool taille_ready = false;

pthread_mutex_t fenetre_mutex;
pthread_cond_t cond_fenetre;
bool fenetre_ready = false;

//sem_t window_size_sem;
//sem_t texture_sem;

pthread_mutex_t texture_rw;
pthread_cond_t texture_r;
pthread_cond_t texture_w;
int nb_textures = 0;

/* l'implantation des fonctions de synchro ici */
void envoiTailleFenetre(th_ycbcr_buffer buffer) {
    pthread_mutex_lock(&taille_mutex);
    windowsx = buffer->width;
    windowsy = buffer->height;
    taille_ready = true;
    pthread_cond_signal(&cond_taille);
    pthread_mutex_unlock(&taille_mutex);
//    sem_post(&window_size_sem);
}

void attendreTailleFenetre() {
    pthread_mutex_lock(&taille_mutex);
    while (!taille_ready)
        pthread_cond_wait(&cond_taille, &taille_mutex);
    pthread_mutex_unlock(&taille_mutex);
//    sem_wait(&window_size_sem);
}

void signalerFenetreEtTexturePrete() {
    pthread_mutex_lock(&fenetre_mutex);
    fenetre_ready = true;
    pthread_cond_signal(&cond_fenetre);
    pthread_mutex_unlock(&fenetre_mutex);
//    sem_post(&texture_sem);
}

void attendreFenetreTexture() {
    pthread_mutex_lock(&fenetre_mutex);
    while (!fenetre_ready)
        pthread_cond_wait(&cond_fenetre, &fenetre_mutex);
    pthread_mutex_unlock(&fenetre_mutex);
//    sem_wait(&texture_sem);
}

void debutConsommerTexture() {
    pthread_mutex_lock(&texture_rw);
    while (nb_textures == 0)
        pthread_cond_wait(&texture_r, &texture_rw);
    pthread_mutex_unlock(&texture_rw);
}

void finConsommerTexture() {
    pthread_mutex_lock(&texture_rw);
    nb_textures--;
    pthread_cond_signal(&texture_w);
    pthread_mutex_unlock(&texture_rw);
}


void debutDeposerTexture() {
    pthread_mutex_lock(&texture_rw);
    while (nb_textures == NBTEX)
        pthread_cond_wait(&texture_w, &texture_rw);
    pthread_mutex_unlock(&texture_rw);
}

void finDeposerTexture() {
    pthread_mutex_lock(&texture_rw);
    nb_textures++;
    pthread_cond_signal(&texture_r);
    pthread_mutex_unlock(&texture_rw);
}
