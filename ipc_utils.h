#ifndef IPC_UTILS_H
#define IPC_UTILS_H

#include "common.h"

// Funkcje Pamięci Dzielonej
int stworz_pamiec(int size);
SharedData* dolacz_pamiec(int shmid);
void odlacz_pamiec(SharedData* data);
void usun_pamiec(int shmid);

// Funkcje Semaforów
int stworz_semafor(int n_sems);
void ustaw_semafor(int semid, int sem_num, int wartosc);
void zablokuj_semafor(int semid, int sem_num);
void zablokuj_semafor_bez_undo(int semid, int sem_num);
void odblokuj_semafor(int semid, int sem_num); 
void odblokuj_semafor_bez_undo(int semid, int sem_num);
int zablokuj_semafor_czekaj(int semid, int sem_num); 
void usun_semafor(int semid);

// Funkcje Kolejki Komunikatów
int stworz_kolejke();
void wyslij_komunikat(int msgid, void* msg, int rozmiar);
void odbierz_komunikat(int msgid, void* msg, int rozmiar, long typ);
void usun_kolejke(int msgid);

#endif