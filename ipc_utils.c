#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <errno.h>
#include "ipc_utils.h"
#include "logs.h"
#include "common.h"

// PAMIĘĆ DZIELONA

int stworz_pamiec(int size) {
    key_t key = ftok(SHM_KEY_PATH, SHM_KEY_ID);

    if (key == -1) {
        loguj_blad("Błąd ftok dla pamięci dzielonej");
        exit(1);
    }
    
    int shmid = shmget(key, size, 0600 | IPC_CREAT);
    if (shmid == -1) {
        loguj_blad("Błąd tworzenia pamięci dzielonej");
        exit(1);
    }
    
    return shmid;
}

SharedData* dolacz_pamiec(int shmid) {
    SharedData* data = (SharedData*)shmat(shmid, NULL, 0);
    
    if (data == (void*)-1) {
        if (errno == EINVAL || errno == EIDRM) exit(0); // Pamięć już została usunięta
        loguj_blad("Błąd dołączania pamięci");
        exit(1);
    }
    
    return data;
}

void odlacz_pamiec(SharedData* data) {
    if (shmdt(data) == -1) {
        if (errno == EINVAL) return; // Już odłączona
        loguj_blad("Błąd odłącznia pamięci");
        exit(1);
    }
}

void usun_pamiec(int shmid) {
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        if (errno == EINVAL) return;
        loguj_blad("Błąd usuwania pamięci");
        exit(1);
    }
}

// SEMAFORY

int stworz_semafor(int n_sems) {
    key_t key = ftok(SHM_KEY_PATH, SEM_KEY_ID);

    if (key == -1) {
        loguj_blad("Błąd ftok dla semafa");
        exit(1);
    }
    
    int semid = semget(key, n_sems, 0600 | IPC_CREAT);
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
    operacja.sem_flg = SEM_UNDO; // Cofnięcie semafora w razie awarii procesu
    
    while (semop(semid, &operacja, 1) == -1) {
        if (errno == EINTR) {
            continue; // To tylko sygnał, próbujemy dalej
        }
        if (errno == EIDRM || errno == EINVAL) {
            exit(0); // Semafor usunięty - koniec symulacji
        }
        loguj_blad("Błąd blokowania semafora (P)");
        exit(1);
    }
}

void zablokuj_semafor_bez_undo(int semid, int sem_num) {
    struct sembuf operacja;
    operacja.sem_num = sem_num;
    operacja.sem_op = -1;
    operacja.sem_flg = 0;
    
    while (semop(semid, &operacja, 1) == -1) {
        if (errno == EINTR) {
            continue; // To tylko sygnał, próbujemy dalej
        }
        if (errno == EIDRM || errno == EINVAL) {
            exit(0); // Semafor usunięty - koniec symulacji
        }
        loguj_blad("Błąd blokowania semafora (P)");
        exit(1);
    }
}

void odblokuj_semafor(int semid, int sem_num) {
    struct sembuf operacja;
    operacja.sem_num = sem_num;
    operacja.sem_op = 1;
    operacja.sem_flg = SEM_UNDO;
    
    while (semop(semid, &operacja, 1) == -1) {
        if (errno == EIDRM || errno == EINVAL) {
            exit(0);
        }
        if (errno == EINTR) continue;
        loguj_blad("Błąd odblokowania semafora (V)");
        exit(1);
    }
}

void odblokuj_semafor_bez_undo(int semid, int sem_num) {
    struct sembuf operacja;
    operacja.sem_num = sem_num;
    operacja.sem_op = 1;
    operacja.sem_flg = 0;
    
    while (semop(semid, &operacja, 1) == -1) {
        if (errno == EIDRM || errno == EINVAL) {
            exit(0);
        }
        if (errno == EINTR) continue;
        loguj_blad("Błąd odblokowania semafora (V)");
        exit(1);
    }
}

// czekanie na semafor, ale z przerwaniem w przypadku sygnału
int zablokuj_semafor_czekaj(int semid, int sem_num) {
    struct sembuf operacja;
    operacja.sem_num = sem_num;
    operacja.sem_op = -1;
    operacja.sem_flg = 0; 

    if (semop(semid, &operacja, 1) == -1) {
        if (errno == EINTR) {
            return -1; // To jest sygnał 
        }
        if (errno == EIDRM || errno == EINVAL) {
            exit(0); // Semafor usunięty
        }
        loguj_blad("Błąd blokowania semafora (P)");
        exit(1);
    }
    return 0;
}

void usun_semafor(int semid) {
    if (semctl(semid, 0, IPC_RMID) == -1) {
        loguj_blad("Błąd usuwania semafora");
        exit(1);
    }
}

// KOLEJKA KOMUNIKATÓW

int stworz_kolejke(int id) {
    key_t key = ftok(SHM_KEY_PATH, id);

    if (key == -1) {
        loguj_blad("Błąd ftok dla kolejki");
        exit(1);
    }
    
    int msgid = msgget(key, 0600 | IPC_CREAT);
    if (msgid == -1) {
        loguj_blad("Błąd tworzenia kolejki komunikatów");
        exit(1);
    }
    
    return msgid;
}

void wyslij_komunikat(int msgid, void* msg, int rozmiar) {
    // 0 = tryb blokujący
    while (msgsnd(msgid, msg, rozmiar, 0) == -1) {
        if (errno == EIDRM || errno == EINVAL) {
            exit(0); 
        }
        if (errno == EINTR) continue;
        loguj_blad("msgsnd"); 
        exit(1);
    }
}

void odbierz_komunikat(int msgid, void* msg, int rozmiar, long typ) {
    while (msgrcv(msgid, msg, rozmiar, typ, 0) == -1) {
        if (errno == EIDRM || errno == EINVAL) {
            exit(0); 
        }
        if (errno == EINTR) continue;
        loguj_blad("msgrcv"); 
        exit(1);
    }
}

void usun_kolejke(int msgid) {
    if (msgctl(msgid, IPC_RMID, NULL) == -1) {
        if (errno == EINVAL) return;
        loguj_blad("Błąd usuwania kolejki komunikatów");
        exit(1);
    }
}