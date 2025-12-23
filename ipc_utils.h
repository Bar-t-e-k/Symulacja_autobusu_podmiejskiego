#ifndef IPC_UTILS_H
#define IPC_UTILS_H

#include "common.h"

// Funkcje Pamięci Dzielonej
int stworz_pamiec(int size);
SharedData* dolacz_pamiec(int shmid);
void odlacz_pamiec(SharedData* data);
void usun_pamiec(int shmid);

// Funkcje Semaforów
int stworz_semafor(int sem_num);
void ustaw_semafor(int semid, int sem_num, int wartosc);
void zablokuj_semafor(int semid, int sem_num);   // Czekaj (P)
void odblokuj_semafor(int semid, int sem_num);   // Zwolnij (V)
void usun_semafor(int semid);

#endif