#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <errno.h>
#include "ipc_utils.h"

// PAMIĘĆ DZIELONA

int stworz_pamiec(int size) {
    key_t key = ftok(SHM_KEY_PATH, SHM_KEY_ID);
    // 0666 = odczyt/zapis dla wszystkich
    int shmid = shmget(key, size, 0666 | IPC_CREAT);
    if (shmid == -1) {
        perror("Błąd tworzenia pamieci dzielonej");
        exit(1);
    }
    return shmid;
}

SharedData* dolacz_pamiec(int shmid) {
    SharedData* data = (SharedData*)shmat(shmid, NULL, 0);
    if (data == (void*)-1) {
        perror("Błąd dolaczania pamieci");
        exit(1);
    }
    return data;
}

void odlacz_pamiec(SharedData* data) {
    if (shmdt(data) == -1) {
        perror("Błąd odlacznia pamieci");
    }
}

void usun_pamiec(int shmid) {
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("Błąd usuwania pamieci");
    }
}

// SEMAFORY

int stworz_semafor(int n_sems) {
    key_t key = ftok(SHM_KEY_PATH, SEM_KEY_ID);
    // Tworzenie n_sems semaforów
    int semid = semget(key, n_sems, 0666 | IPC_CREAT);
    if (semid == -1) {
        perror("Błąd tworzenia semafora");
        exit(1);
    }
    return semid;
}

void ustaw_semafor(int semid, int sem_num, int wartosc) {
    if (semctl(semid, sem_num, SETVAL, wartosc) == -1) {
        perror("Błąd ustawiania semafora");
        exit(1);
    }
}

void zablokuj_semafor(int semid, int sem_num) {
    struct sembuf operacja;
    operacja.sem_num = sem_num;
    operacja.sem_op = -1;
    operacja.sem_flg = 0;
    
    if (semop(semid, &operacja, 1) == -1) {
        perror("Błąd blokowania semafora (P)");
        exit(1);
    }
}

void odblokuj_semafor(int semid, int sem_num) {
    struct sembuf operacja;
    operacja.sem_num = sem_num;
    operacja.sem_op = 1;
    operacja.sem_flg = 0;
    
    if (semop(semid, &operacja, 1) == -1) {
        perror("Błąd odblokowania semafora (V)");
        exit(1);
    }
}

void usun_semafor(int semid) {
    if (semctl(semid, 0, IPC_RMID) == -1) {
        perror("Błąd usuwania semafora");
    }
}