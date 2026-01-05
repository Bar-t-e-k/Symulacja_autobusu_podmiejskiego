#ifndef SIGNALS_H
#define SIGNALS_H

#include "common.h"

extern int g_shmid;
extern int g_semid;
extern int g_msgid;

void obsluga_koniec(int sig);
void obsluga_odjazdu(int sig);
void obsluga_zamkniecia(int sig);

#endif