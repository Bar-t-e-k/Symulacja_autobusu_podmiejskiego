#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

#include "common.h"
#include "ipc_utils.h"
#include "signals.h"

extern int g_shmid;
extern int g_semid;
extern int g_msgid;

void obsluga_koniec(int sig) {
    signal(SIGTERM, SIG_IGN); // Ignoruje kolejne sygnały, żeby nie przerwać sprzątania
    signal(SIGINT, SIG_IGN);

    printf("\n\n[SYSTEM] Otrzymano sygnał kończący (%d). Rozpoczynam procedurę stop.\n", sig);

    if (g_shmid != -1) usun_pamiec(g_shmid);
    if (g_semid != -1) usun_semafor(g_semid);
    if (g_msgid != -1) usun_kolejke(g_msgid);

    // Zabijanie podprocesy
    kill(0, SIGTERM);

    while(wait(NULL) > 0);

    printf("[SYSTEM] Zasoby zwolnione. Koniec.\n");
    exit(0);
}

void obsluga_odjazdu(int sig) {
    (void)sig;

    SharedData* d = dolacz_pamiec(g_shmid);
    
    printf("\n[DYSPOZYTOR ZEWN.] Otrzymano SIGUSR1 -> Rozkaz odjazdu!\n");
    
    if (d->pid_obecnego_autobusu > 0) {
        printf("Wysyłam sygnał do autobusu PID %d\n", d->pid_obecnego_autobusu);
        kill(d->pid_obecnego_autobusu, SIGUSR1);
    } else {
        printf("Brak autobusu na peronie. Rozkaz zignorowany.\n");
    }
    
    odlacz_pamiec(d);
}

void obsluga_zamkniecia(int sig) {
    (void)sig;
    SharedData* d = dolacz_pamiec(g_shmid);
    
    printf("\n[DYSPOZYTOR ZEWN.] Otrzymano SIGUSR2 -> Zamykanie dworca!\n");
    
    if (d->dworzec_otwarty == 1) {
        d->dworzec_otwarty = 0;
        printf("Bramy zamknięte.\n");
        
        if (d->pid_obecnego_autobusu > 0) {
            printf("Wymuszam odjazd obecnego autobusu (PID %d)\n", d->pid_obecnego_autobusu);
            kill(d->pid_obecnego_autobusu, SIGUSR1);
        }
        printf("Czekam na zjazd pozostałych autobusów...\n");
    } else {
        printf("Dworzec już jest zamknięty.\n");
    }
    
    odlacz_pamiec(d);
}