#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

#include "common.h"
#include "ipc_utils.h"
#include "signals.h"
#include "logs.h"

extern int g_shmid;
extern int g_semid;
extern int g_msgid;

// Obsługa sygnału kończącego (SIGINT, SIGTERM)
void obsluga_koniec(int sig) {
    loguj(NULL,"\n\n[SYSTEM] Otrzymano sygnał kończący (%d). Rozpoczynam procedurę stop.\n", sig);

    exit(0);
}

// Obsługa sygnału 1 - rozkaz odjazdu
void obsluga_odjazdu(int sig) {
    (void)sig;

    SharedData* d = dolacz_pamiec(g_shmid);
    
    loguj(NULL,"\n[DYSPOZYTOR ZEWN.] Otrzymano SIGUSR1 -> Rozkaz odjazdu!\n");
    
    if (d->pid_obecnego_autobusu > 0) {
        loguj(NULL,"Wysyłam sygnał do autobusu PID %d\n", d->pid_obecnego_autobusu);
        kill(d->pid_obecnego_autobusu, SIGUSR1);
    } else {
        loguj(NULL,"Brak autobusu na peronie. Rozkaz zignorowany.\n");
    }
    
    odlacz_pamiec(d);
}

// Obsługa sygnału 2 - zamknięcie dworca
void obsluga_zamkniecia(int sig) {
    (void)sig;
    SharedData* d = dolacz_pamiec(g_shmid);
    
    loguj(NULL,"\n[DYSPOZYTOR ZEWN.] Otrzymano SIGUSR2 -> Zamykanie dworca!\n");
    
    if (d->dworzec_otwarty == 1) {
        d->dworzec_otwarty = 0;
        loguj(NULL,"Bramy zamknięte.\n");
        
        if (d->pid_obecnego_autobusu > 0) {
            loguj(NULL,"Wymuszam odjazd obecnego autobusu (PID %d)\n", d->pid_obecnego_autobusu);
            kill(d->pid_obecnego_autobusu, SIGUSR1);
        }
        loguj(NULL,"Czekam na zjazd pozostałych autobusów...\n");
    } else {
        loguj(NULL,"Dworzec już jest zamknięty.\n");
    }
    
    odlacz_pamiec(d);
}