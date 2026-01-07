#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <errno.h>
#include "ipc_utils.h"
#include "logs.h"

// PAMIĘĆ DZIELONA

int stworz_pamiec(int size) {
    key_t key = ftok(SHM_KEY_PATH, SHM_KEY_ID);
    // 0666 = odczyt/zapis dla wszystkich
    int shmid = shmget(key, size, 0666 | IPC_CREAT);
    if (shmid == -1) {
        loguj_blad("Błąd tworzenia pamieci dzielonej");
        exit(1);
    }
    return shmid;
}

SharedData* dolacz_pamiec(int shmid) {
    SharedData* data = (SharedData*)shmat(shmid, NULL, 0);
    if (data == (void*)-1) {
        if (errno == EINVAL || errno == EIDRM) exit(0); // Pamięć już została usunięta
        loguj_blad("Błąd dolaczania pamieci");
        exit(1);
    }
    return data;
}

void odlacz_pamiec(SharedData* data) {
    if (shmdt(data) == -1) {
        if (errno == EINVAL) return; // Już odłączona
        loguj_blad("Błąd odlacznia pamieci");
    }
}

void usun_pamiec(int shmid) {
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        if (errno == EINVAL) return; // Już usunięta
        loguj_blad("Błąd usuwania pamieci");
    }
}

// SEMAFORY

int stworz_semafor(int n_sems) {
    key_t key = ftok(SHM_KEY_PATH, SEM_KEY_ID);
    // Tworzenie n_sems semaforów
    int semid = semget(key, n_sems, 0666 | IPC_CREAT);
    if (semid == -1) {
        loguj_blad("Błąd tworzenia semafora");
        exit(1);
    }
    return semid;
}

void ustaw_semafor(int semid, int sem_num, int wartosc) {
    if (semctl(semid, sem_num, SETVAL, wartosc) == -1) {
        loguj_blad("Błąd ustawiania semafora");
        exit(1);
    }
}

void zablokuj_semafor(int semid, int sem_num) {
    struct sembuf operacja;
    operacja.sem_num = sem_num;
    operacja.sem_op = -1;
    operacja.sem_flg = 0;
    
    if (semop(semid, &operacja, 1) == -1) {
        if (errno == EIDRM || errno == EINVAL || errno == EINTR) {
            exit(0); // Ciche wyjście
        }
        loguj_blad("Błąd blokowania semafora (P)");
        exit(1);
    }
}

void odblokuj_semafor(int semid, int sem_num) {
    struct sembuf operacja;
    operacja.sem_num = sem_num;
    operacja.sem_op = 1;
    operacja.sem_flg = 0;
    
    if (semop(semid, &operacja, 1) == -1) {
        if (errno == EIDRM || errno == EINVAL || errno == EINTR) {
            exit(0); // Ciche wyjście
        }
        loguj_blad("Błąd odblokowania semafora (V)");
        exit(1);
    }
}

void usun_semafor(int semid) {
    if (semctl(semid, 0, IPC_RMID) == -1) {
        loguj_blad("Błąd usuwania semafora");
    }
}

// KOLEJKA KOMUNIKATÓW
int stworz_kolejke() {
    key_t key = ftok(SHM_KEY_PATH, MSG_KEY_ID);
    int msgid = msgget(key, 0666 | IPC_CREAT);
    if (msgid == -1) {
        loguj_blad("Błąd tworzenia kolejki komunikatów");
        exit(1);
    }
    return msgid;
}

void wyslij_komunikat(int msgid, void* msg, int rozmiar) {
    if (msgsnd(msgid, msg, rozmiar, 0) == -1) {
        if (errno == EIDRM || errno == EINVAL || errno == EINTR) {
            exit(0); 
        }
        loguj_blad("msgsnd"); 
        exit(1);
    }
}

void odbierz_komunikat(int msgid, void* msg, int rozmiar, long typ) {
    if (msgrcv(msgid, msg, rozmiar, typ, 0) == -1) {
        if (errno == EIDRM || errno == EINVAL || errno == EINTR) {
            exit(0); 
        }
        loguj_blad("msgrcv"); 
        exit(1);
    }
}

void usun_kolejke(int msgid) {
    if (msgctl(msgid, IPC_RMID, NULL) == -1) {
        if (errno == EINVAL) return; // Już usunięta
        loguj_blad("Błąd usuwania kolejki komunikatów");
    }
}

int odbierz_komunikat_bez_blokowania(int msgid, void* msg, int rozmiar, long typ) {
    if (msgrcv(msgid, msg, rozmiar, typ, IPC_NOWAIT) == -1) {
        if (errno == ENOMSG) {
            return 0;
        }
        if (errno == EIDRM || errno == EINVAL || errno == EINTR) exit(0);
        loguj_blad("msgrcv non-blocking");
        exit(1);
    }
    return 1;
}