#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <signal.h>
#include "common.h"
#include "ipc_utils.h"
#include "actors.h"
#include "signals.h"

int g_shmid, g_semid, g_msgid;

int main() {
    printf("Symulacja dworca..\n");

    signal(SIGUSR1, obsluga_odjazdu);
    signal(SIGUSR2, obsluga_zamkniecia);
    signal(SIGINT, obsluga_koniec);  
    signal(SIGTERM, obsluga_koniec);

    // 1. Tworzenie zasobów
    g_shmid = stworz_pamiec(sizeof(SharedData));
    g_semid = stworz_semafor(LICZBA_SEMAFOROW);
    g_msgid = stworz_kolejke();

    // 2. Inicjalizacja
    SharedData* data = dolacz_pamiec(g_shmid);
    data->liczba_pasazerow = 0;
    data->liczba_rowerow = 0;
    data->liczba_vip_oczekujacych = 0;
    data->autobus_obecny = 0;
    data->calkowita_liczba_pasazerow = 0;
    data->pasazerowie_obsluzeni = 0;
    data->aktywne_autobusy = N;
    data->limit_pasazerow = LICZBA_PASAZEROW;
    data->dworzec_otwarty = 1;
    data->pid_obecnego_autobusu = 0;
    odlacz_pamiec(data);

    ustaw_semafor(g_semid, SEM_MUTEX, 1); // MUTEX otwarty
    ustaw_semafor(g_semid, SEM_DRZWI_PAS, 1);  // Drzwi pasażerów
    ustaw_semafor(g_semid, SEM_DRZWI_ROW, 1);  // Drzwi rowerów

    // 3. Kasjer
    if (fork() == 0) {
        signal(SIGINT, SIG_IGN); 
        signal(SIGTERM, SIG_DFL);
        signal(SIGUSR1, SIG_IGN); 
        signal(SIGUSR2, SIG_IGN);

        kasjer_run(g_msgid);
    }

    // 4. Autobusy
    for (int b = 1; b <= N; b++) {
        if (fork() == 0) {
            signal(SIGINT, SIG_IGN); 
            signal(SIGTERM, SIG_DFL);
            signal(SIGUSR2, SIG_IGN);

            autobus_run(b, g_shmid, g_semid);
        }
    }

    // 5. Pasażerowie
    if (fork() == 0) {
        signal(SIGINT, SIG_IGN); 
        signal(SIGTERM, SIG_DFL);
        srand(time(NULL));

        for (int i = 0; i < LICZBA_PASAZEROW; i++) {
            SharedData* d = dolacz_pamiec(g_shmid);
            if (d->dworzec_otwarty == 0) {
                odlacz_pamiec(d);
                break;
            }
            odlacz_pamiec(d);

            int los = rand() % 100;
            int typ = TYP_ZWYKLY;       

            if (los < 20) typ = TYP_VIP;         // 20% VIP
            else if (los < 50) typ = TYP_ROWER;  // 30% Rower
                
            if (fork() == 0) {
                pasazer_run(i + 1, g_shmid, g_semid, g_msgid, typ);
            }
            usleep(200000); // Nowy pasażer co 0.2s
        }
        exit(0);
    }

    // 6. Dyspozytor
    if (fork() == 0) {
        signal(SIGINT, SIG_IGN); 
        signal(SIGTERM, SIG_DFL);
        signal(SIGUSR1, SIG_IGN); 
        signal(SIGUSR2, SIG_IGN);

        signal(SIGTTIN, SIG_IGN);

        char bufor[10];

        while(1) {
            if (fgets(bufor, sizeof(bufor), stdin)) {
                if (bufor[0] == '1') {
                    kill(getppid(), SIGUSR1);
                }
                else if (bufor[0] == '2') {
                    kill(getppid(), SIGUSR2);
                }
                else if (bufor[0] == 'q') {
                    kill(getppid(), SIGTERM);
                    exit(0);
                }
            }
        }
        exit(0);
    }

    // Oczekiwanie na zakończenie wszystkich autobusów
    while(1) {
        data = dolacz_pamiec(g_shmid);
        if (data->aktywne_autobusy == 0) {
            odlacz_pamiec(data);
            printf("\n[MAIN] Wszystkie autobusy zjechały.\n");
            break; 
        }
        odlacz_pamiec(data);
        sleep(1);
    }

    // 7. Raport końcowy
    data = dolacz_pamiec(g_shmid);
    printf("\n--- RAPORT KOŃCOWY ---\n");
    printf("Łącznie obsłużono pasażerów: %d\n", data->calkowita_liczba_pasazerow);
    odlacz_pamiec(data);

    // 8. Sprzątanie
    obsluga_koniec(0);

    return 0;
}