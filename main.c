#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include "common.h"
#include "ipc_utils.h"
#include "logs.h"
#include "config.h"

pid_t g_main_pid;
int g_shmid = -1, g_semid = -1, g_msgid_req = -1, g_msgid_res = -1;
volatile sig_atomic_t flaga_odjazd = 0;
volatile sig_atomic_t flaga_zamkniecie = 0;
volatile sig_atomic_t flaga_koniec = 0;

// Handler sygnałów
// Przechwytuje sygnały systemowe.
// Ustawia flagi, aby główna pętla programu odczytała je i podjęła akcję.
void handler_sygnalow(int sig) {
    // Obsługa sygnału kończącego Ctrl+C
    // Wywołuje exit, co uruchamia sprzątanie zasobów w main.c
    if (sig == SIGINT || sig == SIGTERM)  flaga_koniec = 1;

    // Obsługa sygnału 1 - rozkaz odjazdu
    // Wysyła sygnał do obecnego autobusu na peronie
    if (sig == SIGUSR1) flaga_odjazd = 1;

    // Obsługa sygnału 2 - zamknięcie dworca
    // Ustawia flagę zamknięcia i wymusza odjazd obecnego autobusu
    if (sig == SIGUSR2) flaga_zamkniecie = 1;
}

// Funkcja sprzątająca zasoby przed zakończeniem programu (wywoływana przez atexit)
void sprzatanie() {
    if (getpid() != g_main_pid) return;

    ustaw_sygnal(SIGTERM, SIG_IGN, 1);
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

    ustaw_sygnal(SIGUSR1, handler_sygnalow, 0);
    ustaw_sygnal(SIGUSR2, handler_sygnalow, 0);
    ustaw_sygnal(SIGINT, handler_sygnalow, 0);
    ustaw_sygnal(SIGCHLD, handler_sygnalow, 0);
    ustaw_sygnal(SIGTERM, handler_sygnalow, 0);

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
    data->liczba_rowerow_oczekujacych = 0;

    int N = data->cfg_N;
    odlacz_pamiec(data);

    // Ustawienia semaforów
    ustaw_semafor(g_semid, SEM_MUTEX, 1); 
    ustaw_semafor(g_semid, SEM_DRZWI_PAS, 0);  
    ustaw_semafor(g_semid, SEM_DRZWI_ROW, 0);
    ustaw_semafor(g_semid, SEM_KOLEJKA_VIP, 0);
    ustaw_semafor(g_semid, SEM_WSIADL, 0);
    ustaw_semafor(g_semid, SEM_PRZYSTANEK, 1);
    ustaw_semafor(g_semid, SEM_KTOS_CZEKA, 0);
    ustaw_semafor(g_semid, SEM_LIMIT, 3000);

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
        ustaw_sygnal(SIGINT, SIG_IGN, 1);
        ustaw_sygnal(SIGTERM, SIG_DFL, 1);
        ustaw_sygnal(SIGUSR1, SIG_IGN, 1);
        ustaw_sygnal(SIGUSR2, SIG_IGN, 1);

        execlp("./exe_cashier", "exe_cashier", s_msg_req, s_msg_res, NULL);

        loguj_blad("Exec Kasjer");
        exit(1);
    } else if (pid_kasjer < 0) {
        loguj_blad("Fork Kasjer");
        exit(1);
    }

    // 4. Autobusy
    for (int b = 1; b <= N; b++) {
        pid_t pid_autobus = fork();
        if (pid_autobus == 0) {
            ustaw_sygnal(SIGINT, SIG_IGN, 1);
            ustaw_sygnal(SIGTERM, SIG_DFL, 1);
            ustaw_sygnal(SIGUSR2, SIG_IGN, 1);

            char s_id[16];
            sprintf(s_id, "%d", b);

            execlp("./exe_bus", "exe_bus", s_id, s_shm, s_sem, NULL);

            loguj_blad("Exec Autobus");
            exit(1);
        } else if (pid_autobus < 0) {
            loguj_blad("Fork Autobus");
            exit(1);
        }
    }

    // 5. Pasażerowie
    // Najpierw towrzony generator, który potem tworzy pasażerów
    pid_t pid_pasazerowie = fork();
    if (pid_pasazerowie == 0) {
        ustaw_sygnal(SIGINT, SIG_IGN, 1);
        ustaw_sygnal(SIGTERM, SIG_DFL, 1);
        ustaw_sygnal(SIGCHLD, SIG_IGN, 1);
        srand(time(NULL));
        int id_gen = 1;

        char s_id[16], s_typ[16];

        while (1) {
            SharedData* d = dolacz_pamiec(g_shmid);
            if (d->dworzec_otwarty == 0) {
                odlacz_pamiec(d);
                break;
            }
            odlacz_pamiec(d);

            int los = rand() % 100;
            int typ = TYP_ZWYKLY;
                 
            if (los < 20) { 
                typ = TYP_OPIEKUN;
            } else if (los < 45) {
                typ = TYP_ROWER;
            } else if (los >= 99) { // 1% szans na VIP
                typ = TYP_VIP;
            }

            zablokuj_semafor_bez_undo(g_semid, SEM_LIMIT);

            pid_t pid_pas = fork();
            if (pid_pas == 0) {
                sprintf(s_id, "%d", id_gen);
                sprintf(s_typ, "%d", typ);

                execlp("./exe_passenger", "exe_passenger", s_id, s_shm, s_sem, s_msg_req, s_msg_res, s_typ, NULL);

                loguj_blad("Exec Pasażer"); 
                odblokuj_semafor_bez_undo(g_semid, SEM_LIMIT);
                exit(1);
            } else if (pid_pas < 0) {
                loguj_blad("Fork Pasażer");

                odblokuj_semafor_bez_undo(g_semid, SEM_LIMIT);
                
                kill(getppid(), SIGINT);
                exit(1);
            }

            id_gen++;
            //usleep(200000 + (rand()%200000)); // Nowy pasażer co losowy odstęp
            }
            exit(0);   
    } else if (pid_pasazerowie < 0) {
        loguj_blad("Fork Generator pasażerów");
        loguj_blad("Spróbuj zmniejszyć liczbę autobusów");
        exit(1);
    }

    // 6. Dyspozytor
    // Czyta dane z klawiatury
    pid_t pid_dyspozytor = fork();
    if (pid_dyspozytor == 0) {
        ustaw_sygnal(SIGINT, SIG_IGN, 1);
        ustaw_sygnal(SIGTERM, SIG_DFL, 1);
        ustaw_sygnal(SIGUSR1, SIG_IGN, 1);
        ustaw_sygnal(SIGUSR2, SIG_IGN, 1);
        ustaw_sygnal(SIGTTIN, SIG_IGN, 1);

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
        exit(1);
    }

    // 7. Pętla główna - monitorowanie stanu i obsługa sygnałów
    while(1) {
        // Obsługa sygnałów
        if (flaga_koniec) {
            loguj(NULL,"\n\n[SYSTEM] Otrzymano sygnał kończący (Ctrl + C). Rozpoczynam procedurę stop.\n");
            exit(0); 
        }
        
        zablokuj_semafor(g_semid, SEM_MUTEX);
        data = dolacz_pamiec(g_shmid);
        if (data == NULL) break;

        if (flaga_odjazd) {
            loguj(NULL,"\n[DYSPOZYTOR ZEWN.] Otrzymano SIGUSR1 -> Rozkaz odjazdu!\n");
        
            if (data->pid_obecnego_autobusu > 0) {
                loguj(NULL,"Wysyłam sygnał do autobusu PID %d\n", data->pid_obecnego_autobusu);
                kill(data->pid_obecnego_autobusu, SIGUSR1);
            } else {
                loguj(NULL,"Brak autobusu na peronie. Rozkaz zignorowany.\n");
            }

            flaga_odjazd = 0;
        }

        if (flaga_zamkniecie) {
            if (data->dworzec_otwarty == 1) {
                data->dworzec_otwarty = 0;
                loguj(NULL,"Bramy zamknięte.\n");

                for(int i = 0; i < data->cfg_N; i++) {
                    odblokuj_semafor(g_semid, SEM_PRZYSTANEK); 
                    odblokuj_semafor(g_semid, SEM_KTOS_CZEKA);
                }
                
                if (data->pid_obecnego_autobusu > 0) {
                    loguj(NULL,"Wymuszam odjazd obecnego autobusu (PID %d)\n", data->pid_obecnego_autobusu);
                    kill(data->pid_obecnego_autobusu, SIGUSR1);
                }
                loguj(NULL,"Czekam na zjazd pozostałych autobusów...\n");
            } else {
                loguj(NULL,"Dworzec już jest zamknięty.\n");
            }
            flaga_zamkniecie = 0;
        }

        // Sprawdzenie czy wszystkie autobusy zakończyły pracę
        if (data->aktywne_autobusy == 0) {
            odlacz_pamiec(data);
            odblokuj_semafor(g_semid, SEM_MUTEX);
            loguj(NULL,"[MAIN] Wszystkie autobusy zjechały.\n");
            break; 
        }
        odlacz_pamiec(data);
        odblokuj_semafor(g_semid, SEM_MUTEX);
        
        //pause();
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