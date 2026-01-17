#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include "common.h"
#include "ipc_utils.h"
#include "signals.h"
#include "logs.h"
#include "config.h"

pid_t g_main_pid;
int g_shmid = -1, g_semid = -1, g_msgid_req = -1, g_msgid_res = -1;

// Funkcja sprzątająca zasoby przed zakończeniem programu (wywoływana przez atexit)
void sprzatanie() {
    if (getpid() != g_main_pid) return;

    signal(SIGTERM, SIG_IGN);
    kill(0, SIGTERM); 

    pid_t wpid;
    int status;
    while ((wpid = waitpid(-1, &status, WNOHANG)) > 0);

    if (g_shmid != -1) {
        usun_pamiec(g_shmid);
        g_shmid = -1; 
    }
    if (g_semid != -1) {
        usun_semafor(g_semid);
        g_semid = -1;
    }
    if (g_msgid_req != -1) {
        usun_kolejke(g_msgid_req);
        g_msgid_req = -1;
    }
    if (g_msgid_res != -1) {
        usun_kolejke(g_msgid_res);
        g_msgid_res = -1;
    }
    
    loguj(NULL, "[SYSTEM] Zasoby posprzątane. Koniec.\n");
}

int main() {
    g_main_pid = getpid();
    atexit(sprzatanie);

    FILE* f = fopen("symulacja.log", "w");
    if (f) fclose(f);

    time_t now = time(NULL);
    struct tm* timeinfo = localtime(&now);
    char datetime[64];
    strftime(datetime, sizeof(datetime), "%Y-%m-%d %H:%M:%S", timeinfo);
    loguj(NULL,"Symulacja dworca - %s\n", datetime);

    signal(SIGUSR1, obsluga_odjazdu);
    signal(SIGUSR2, obsluga_zamkniecia);
    signal(SIGINT, obsluga_koniec);  
    signal(SIGTERM, obsluga_koniec);

    // 1. Tworzenie zasobów IPC
    g_shmid = stworz_pamiec(sizeof(SharedData));
    g_semid = stworz_semafor(LICZBA_SEMAFOROW);
    g_msgid_req = stworz_kolejke(MSG_KEY_ID_REQ);
    g_msgid_res = stworz_kolejke(MSG_KEY_ID_RES);

    if (g_shmid == -1 || g_semid == -1 || g_msgid_req == -1 || g_msgid_res == -1) {
        loguj_blad("Nie udało sie utworzyć zasobów IPC!");
        exit(1);
    }

    // 2. Inicjalizacja i konfiguracja pamięci współdzielonej
    SharedData* data = dolacz_pamiec(g_shmid);

    wczytaj_konfiguracje("config.txt", data);
    waliduj_konfiguracje(data);    

    data->liczba_pasazerow = 0;
    data->liczba_rowerow = 0;
    data->autobus_obecny = 0;
    data->calkowita_liczba_pasazerow = 0;
    data->aktywne_autobusy = data->cfg_N;
    data->dworzec_otwarty = 1;
    data->pid_obecnego_autobusu = 0;
    data->liczba_oczekujacych = 0;
    data->liczba_vip_oczekujacych = 0;

    int N = data->cfg_N;
    int P = data->cfg_LiczbaPas;
    odlacz_pamiec(data);

    ustaw_semafor(g_semid, SEM_MUTEX, 1); 
    ustaw_semafor(g_semid, SEM_DRZWI_PAS, 1);  
    ustaw_semafor(g_semid, SEM_DRZWI_ROW, 1); 

    // Konwersja ID na stringi dla exec
    char s_shm[16], s_sem[16], s_msg_req[16], s_msg_res[16];
    sprintf(s_shm, "%d", g_shmid);
    sprintf(s_sem, "%d", g_semid);
    sprintf(s_msg_req, "%d", g_msgid_req);
    sprintf(s_msg_res, "%d", g_msgid_res);

    // 3. Kasjer
    pid_t pid_kasjer = fork();
    if (pid_kasjer == 0) {
        // Ignorowanie sygnałów terminala, aby nie przerywać pracy
        signal(SIGINT, SIG_IGN); 
        signal(SIGTERM, SIG_DFL);
        signal(SIGUSR1, SIG_IGN); 
        signal(SIGUSR2, SIG_IGN);

        execlp("./exe_cashier", "exe_cashier", s_msg_req, s_msg_res, NULL);

        loguj_blad("Exec Kasjer");
        exit(1);
    } else if (pid_kasjer < 0) {
        loguj_blad("Fork Kasjer");
    }

    // 4. Autobusy
    for (int b = 1; b <= N; b++) {
        pid_t pid_autobus = fork();
        if (pid_autobus == 0) {
            signal(SIGINT, SIG_IGN); 
            signal(SIGTERM, SIG_DFL);
            signal(SIGUSR2, SIG_IGN);

            char s_id[16];
            sprintf(s_id, "%d", b);

            execlp("./exe_bus", "exe_bus", s_id, s_shm, s_sem, NULL);

            loguj_blad("Exec Autobus");
            exit(1);
        } else if (pid_autobus < 0) {
            loguj_blad("Fork Autobus");
        }
    }

    // 5. Pasażerowie
    pid_t pid_pasazerowie = fork();
    if (pid_pasazerowie == 0) {
        signal(SIGINT, SIG_IGN); 
        signal(SIGTERM, SIG_DFL);
        signal(SIGCHLD, SIG_IGN);
        srand(time(NULL));
        int id_gen = 1;

        char s_id[16], s_typ[16];

        for (int i = 0; i < P; i++) {
            SharedData* d = dolacz_pamiec(g_shmid);
            if (d->dworzec_otwarty == 0) {
                odlacz_pamiec(d);
                break;
            }
            odlacz_pamiec(d);

            int los = rand() % 100;
            int typ = TYP_ZWYKLY;
                 
            if (los < 20 && i < P - 1) { 
                typ = TYP_OPIEKUN;
                i++; // Zliczanie dwóch miejsc (Opiekun + Dziecko)
            } else if (los < 45) {
                typ = TYP_ROWER;
            } else if (los >= 99) { // 1% szans na VIP
                typ = TYP_VIP;
            }

            pid_t pid_pas = fork();
            if (pid_pas == 0) {
                sprintf(s_id, "%d", id_gen);
                sprintf(s_typ, "%d", typ);
                execlp("./exe_passenger", "exe_passenger", s_id, s_shm, s_sem, s_msg_req, s_msg_res, s_typ, NULL);
                loguj_blad("Exec Pasażer"); 
                exit(1);
            }

            id_gen++;
            usleep(200000 + (rand()%200000)); // Nowy pasażer co losowy odstęp
            }
            while(wait(NULL) > 0);
            exit(0);   
    } else if (pid_pasazerowie < 0) {
        loguj_blad("Fork Pasażerowie");
    }

    // 6. Dyspozytor
    pid_t pid_dyspozytor = fork();
    if (pid_dyspozytor == 0) {
        signal(SIGINT, SIG_IGN); 
        signal(SIGTERM, SIG_DFL);
        signal(SIGUSR1, SIG_IGN); 
        signal(SIGUSR2, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);

        char bufor[32];

        while(1) {
            if (fgets(bufor, sizeof(bufor), stdin) == NULL) {
                break;
            }

            size_t len = strlen(bufor);
            if (len > 0 && bufor[len - 1] == '\n') {
                bufor[len - 1] = '\0';
                len--;
            }

            if (len == 0) {
                continue;
            }

            if (strcmp(bufor, "1") == 0) {
                kill(getppid(), SIGUSR1);
            } else if (strcmp(bufor, "2") == 0) {
                kill(getppid(), SIGUSR2);
            } else {
                loguj_blad("[DYSPOZYTOR] Nieznana komenda");
            }
        }
        exit(0);
    } else if (pid_dyspozytor < 0) {
        loguj_blad("Fork Dyspozytor");
    }

    // 7. Pętla główna - monitorowanie stanu
    while(1) {
        data = dolacz_pamiec(g_shmid);
        if (data == NULL) break;
        
        // Zamknięcie dworca po obsłużeniu limitu pasażerów
        if (data->calkowita_liczba_pasazerow >= P && data->liczba_oczekujacych == 0) {
            if (data->dworzec_otwarty == 1) {
                loguj(NULL,"[MAIN] Wszyscy pasażerowie obsłużeni (%d). Zamykam dworzec!\n", data->calkowita_liczba_pasazerow);
                data->dworzec_otwarty = 0;
            }
        }

        if (data->aktywne_autobusy == 0) {
            odlacz_pamiec(data);
            loguj(NULL,"[MAIN] Wszystkie autobusy zjechały.\n");
            break; 
        }
        odlacz_pamiec(data);
        sleep(1);
    }

    // Raport końcowy
    data = dolacz_pamiec(g_shmid);
    if (data != NULL) {
        loguj(NULL,"--- RAPORT KOŃCOWY ---\n");
        loguj(NULL,"Łącznie obsłużono pasażerów: %d\n", data->calkowita_liczba_pasazerow);
        odlacz_pamiec(data);
    }

    return 0;
}