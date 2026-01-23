#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include "common.h"
#include "ipc_utils.h"
#include "logs.h"
#include "config.h"

// ZARZĄDZANIE PROCESAMI I FLAGI

// Zmienne na PID-y, aby móc precyzyjnie wysyłać sygnały sterujące
// do konkretnych komponentów.
pid_t g_pid_kasjer = 0;
pid_t g_pid_generator = 0;
pid_t g_pid_dyspozytor = 0;
pid_t* g_pids_autobusy = NULL; 
int g_liczba_autobusow_cfg = 0;

// Flagi stanów używane m.in. w handlerach
volatile int g_sprzatacz_pracuje = 1;
volatile sig_atomic_t generator_uruchomiony = 1;
volatile sig_atomic_t flaga_odjazd = 0;
volatile sig_atomic_t flaga_zamkniecie = 0;
volatile sig_atomic_t flaga_koniec = 0;
volatile sig_atomic_t flaga_awaria = 0;

// Flaga blokująca raportowanie awarii podczas planowanego wyłączania systemu
volatile int g_system_zamykany = 0;

pid_t g_main_pid;
int g_shmid = -1, g_semid = -1, g_msgid_req = -1, g_msgid_res = -1;

pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_t g_thread_sprzatacz;

// Obsługa sygnałów: tylko ustawianie flag
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

void stop_generator(int sig) {
    generator_uruchomiony = 0;
}

int czy_to_autobus(pid_t pid) {
    if (g_pids_autobusy == NULL) return 0;
    for(int i=0; i < g_liczba_autobusow_cfg; i++) {
        if (g_pids_autobusy[i] == pid) return 1;
    }
    return 0;
}

// WĄTEK CZYSZCZĄCY
// Natychmiastowo usuwa procesy Zombie po dzieciach głównego procesu.
// Monitoruje również 'zdrowie' symulacji - jeśli procesy krytyczne padną
// bez wezwania, wątek zgłasza awarię do pętli głównej.
void* watek_czyszczacy_fun(void* arg) {
    (void)arg;
    int status;
    pid_t pid;

    while (1) {
        pid = waitpid(-1, &status, 0); // Blokuje wątek do czasu zakończenia dowolnego dziecka

        if (pid <= 0) {
            if (errno == ECHILD) {
                if (g_sprzatacz_pracuje == 1) {
                    continue; 
                } else {
                    return NULL;
                }
            }
            continue;
        }

        // Sprawdzenie, czy zakończony proces był krytyczny dla działania dworca
        int krytyczny = 0;
        char* kto = ""; 

        if (pid == g_pid_kasjer) { kto = "KASJER"; krytyczny = 1; }
        else if (pid == g_pid_generator) { kto = "GENERATOR"; krytyczny = 1; }
        else if (pid == g_pid_dyspozytor) { kto = "DYSPOZYTOR"; krytyczny = 1; }
        else if (czy_to_autobus(pid)) { kto = "AUTOBUS"; krytyczny = 1; }

        int kod_wyjscia = -1;
        if (WIFEXITED(status)) kod_wyjscia = WEXITSTATUS(status);

        // Logika awarii (jeśli padł ważny proces w trakcie pracy)
        if (krytyczny && !g_system_zamykany) {
            pthread_mutex_lock(&log_mutex);
            loguj_blad("Proces krytyczny zakończony niespodziewanie");
            if (WIFSIGNALED(status)) {
                loguj(NULL, "Proces %s (PID: %d) został zabity sygnałem %d\n", kto, pid, WTERMSIG(status));
            } else {
                loguj(NULL, "Proces %s (PID: %d) zakończył się błędem (Kod: %d)\n", kto, pid, kod_wyjscia);
            }
            pthread_mutex_unlock(&log_mutex);
            flaga_awaria = 1; 
        }
    }
    return NULL;
}

// Funkcja sprzątająca zasoby przed zakończeniem programu (wywoływana przez atexit)
void sprzatanie() {
    if (getpid() != g_main_pid) return;

    ustaw_sygnal(SIGTERM, SIG_IGN, 1);

    kill(0, SIGTERM);

    int status;
    while (waitpid(-1, &status, 0) > 0);

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

    if (g_pids_autobusy != NULL) { 
        free(g_pids_autobusy); g_pids_autobusy = NULL; 
    }
    
    loguj(NULL, "[SYSTEM] Zasoby posprzątane. Koniec.\n");
}

int main() {
    g_main_pid = getpid();
    atexit(sprzatanie);

    int fd = open("symulacja.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd != -1) {
        close(fd);
    }

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
    data->aktywne_autobusy = g_liczba_autobusow_cfg = data->cfg_N;
    g_pids_autobusy = malloc(sizeof(pid_t) * g_liczba_autobusow_cfg);
    if (g_pids_autobusy == NULL) {
         loguj_blad("Błąd malloc pids");
         exit(1);
    }
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

    // Tworzenie wątku sprzątającego
    if (pthread_create(&g_thread_sprzatacz, NULL, watek_czyszczacy_fun, NULL) != 0) {
        loguj_blad("Błąd tworzenia wątku Reapera");
        exit(1);
    }

    // 3. Kasjer
    g_pid_kasjer = fork();
    if (g_pid_kasjer == 0) {
        // Ignorowanie sygnałów terminala, aby nie przerywać pracy
        ustaw_sygnal(SIGINT, SIG_IGN, 1);
        ustaw_sygnal(SIGUSR1, SIG_IGN, 1);
        ustaw_sygnal(SIGUSR2, SIG_IGN, 1);

        execlp("./exe_cashier", "exe_cashier", s_msg_req, s_msg_res, NULL);

        loguj_blad("Exec Kasjer");
        exit(1);
    } else if (g_pid_kasjer < 0) {
        loguj_blad("Fork Kasjer");
        exit(1);
    }

    // 4. Autobusy
    for (int b = 1; b <= N; b++) {
        g_pids_autobusy[b-1] = fork();
        if (g_pids_autobusy[b-1] == 0) {
            ustaw_sygnal(SIGINT, SIG_IGN, 1);
            ustaw_sygnal(SIGUSR2, SIG_IGN, 1);

            char s_id[16];
            sprintf(s_id, "%d", b);

            execlp("./exe_bus", "exe_bus", s_id, s_shm, s_sem, NULL);

            loguj_blad("Exec Autobus");
            exit(1);
        } else if (g_pids_autobusy[b-1] < 0) {
            loguj_blad("Fork Autobus");
            exit(1);
        }
    }

    // 5. Pasażerowie
    // Najpierw tworzony generator, który potem tworzy pasażerów
    g_pid_generator = fork();
    if (g_pid_generator == 0) {
        ustaw_sygnal(SIGINT, SIG_IGN, 1);
        ustaw_sygnal(SIGTERM, stop_generator, 0);
        ustaw_sygnal(SIGUSR1, SIG_IGN, 1);
        ustaw_sygnal(SIGUSR2, SIG_IGN, 1);

        // Ze względu na to, że wątek sprzątający nie ma możliwości
        // czyszczenia Zombie pasażerów, a procesy tego typu
        // nie są krytyczne do działania symulacji, są one sprzątane przez system automatycznie
        ustaw_sygnal(SIGCHLD, SIG_IGN, 1); 
        srand(time(NULL));
        int id_gen = 1;

        char s_id[16], s_typ[16];

        // Pętla generująca pasażerów
        while (generator_uruchomiony) {
            zablokuj_semafor(g_semid, SEM_MUTEX);
            SharedData* d = dolacz_pamiec(g_shmid);
            if (d->dworzec_otwarty == 0) {
                odlacz_pamiec(d);
                break;
            }
            odlacz_pamiec(d);
            odblokuj_semafor(g_semid, SEM_MUTEX);

            int los = rand() % 100;
            int typ = TYP_ZWYKLY;
                 
            if (los < 20) { 
                typ = TYP_OPIEKUN;
            } else if (los < 45) {
                typ = TYP_ROWER;
            } else if (los >= 99) { // 1% szans na VIP
                typ = TYP_VIP;
            }

            // Czekanie na wolne miejsce na semaforze
            if (zablokuj_semafor_czekaj(g_semid, SEM_LIMIT) == -1) {
                if (errno == EINTR && !generator_uruchomiony) {
                    break;
                }
                continue;
            }

            if (!generator_uruchomiony) break;

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

                odblokuj_semafor(g_semid, SEM_LIMIT);
                
                kill(getppid(), SIGINT);
                exit(1);
            }

            id_gen++;
            usleep(200000 + (rand()%200000)); // Nowy pasażer co losowy odstęp
            }
            exit(0);   
    } else if (g_pid_generator < 0) {
        loguj_blad("Fork Generator pasażerów");
        loguj_blad("Spróbuj zmniejszyć liczbę autobusów");
        exit(1);
    }

    // 6. Dyspozytor
    // Czyta sygnały z klawiatury
    g_pid_dyspozytor = fork();
    if (g_pid_dyspozytor == 0) {
        ustaw_sygnal(SIGINT, SIG_IGN, 1);
        ustaw_sygnal(SIGTERM, handler_sygnalow, 0);
        ustaw_sygnal(SIGUSR1, SIG_IGN, 1);
        ustaw_sygnal(SIGUSR2, SIG_IGN, 1);
        ustaw_sygnal(SIGTTIN, SIG_IGN, 1);

        char bufor[32];

        while(1) {
            if (flaga_koniec) break;

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
    } else if (g_pid_dyspozytor < 0) {
        loguj_blad("Fork Dyspozytor");
        exit(1);
    }

    // 7. Pętla główna - monitorowanie stanu i obsługa sygnałów
    while(1) {
        // Obsługa sygnałów

        // Reakcja na problem wykryty przez wątek sprzątający
        if (flaga_awaria && !flaga_koniec) {
            loguj(NULL, "[MAIN] Wykryto usunięcie krytycznego procesu. Procedura stop\n");
            flaga_koniec = 1; 
        }

        // Obsługa Ctrl + C
        if (flaga_koniec) {
            g_system_zamykany = 1;

            loguj(NULL,"\n\n[SYSTEM] Otrzymano sygnał kończący (Ctrl + C). Rozpoczynam procedurę stop.\n");

            g_sprzatacz_pracuje = 0;
            ustaw_sygnal(SIGTERM, SIG_IGN, 1);
            kill(0, SIGTERM);

            pthread_join(g_thread_sprzatacz, NULL);
            break;
        }
        
        zablokuj_semafor(g_semid, SEM_MUTEX);
        data = dolacz_pamiec(g_shmid);
        if (data == NULL) break;

        // Obsługa SIGUSR1
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

        // Logika podstawowego zamykania dworca (SIGUSR2)
        // 1. Ustawienie flagi 'dworzec_otwarty' na 0 (blokada wejść).
        // 2. Budzenie uśpionych procesów przez 'V' na semaforach.
        // 3. Wysłanie sygnału wyjścia do wszystkich procesów pasażerów.
        if (flaga_zamkniecie) {
            if (data->dworzec_otwarty == 1) {
                g_system_zamykany = 1;
                data->dworzec_otwarty = 0;
                loguj(NULL,"Bramy zamknięte.\n");

                for(int i = 0; i < data->cfg_N; i++) {
                    odblokuj_semafor(g_semid, SEM_PRZYSTANEK); 
                    odblokuj_semafor_bez_undo(g_semid, SEM_KTOS_CZEKA);
                }
                
                if (data->pid_obecnego_autobusu > 0) {
                    loguj(NULL,"Wymuszam odjazd obecnego autobusu (PID %d)\n", data->pid_obecnego_autobusu);
                    kill(data->pid_obecnego_autobusu, SIGUSR1);
                }
                kill(0, SIGUSR2);
                loguj(NULL,"Czekam na zjazd pozostałych autobusów...\n");
            } else {
                loguj(NULL,"Dworzec już jest zamknięty.\n");
            }
            flaga_zamkniecie = 0;
        }

        // Warunek wyjścia: Dworzec zamknięty i flota autobusowa wróciła do zajezdni (aktywne == 0)
        if (data->aktywne_autobusy == 0 && data->dworzec_otwarty == 0) {
            odlacz_pamiec(data);
            odblokuj_semafor(g_semid, SEM_MUTEX);
            loguj(NULL,"[MAIN] Wszystkie autobusy zjechały.\n");

            g_sprzatacz_pracuje = 0;
            ustaw_sygnal(SIGTERM, SIG_IGN, 1);
            kill(0, SIGTERM);

            pthread_join(g_thread_sprzatacz, NULL);
            break; 
        }
        odlacz_pamiec(data);
        odblokuj_semafor(g_semid, SEM_MUTEX);
        
        pause();
    }

    // Raport końcowy
    data = dolacz_pamiec(g_shmid);
    if (data != NULL) {
        loguj(NULL,"--- RAPORT KOŃCOWY ---\n");
        loguj(NULL,"Łącznie obsłużono pasażerów: %d\n", data->calkowita_liczba_pasazerow);
        loguj(NULL,"Wyszlo: %d\n", data->liczba_wyjsc);
        odlacz_pamiec(data);
    }

    return 0;
}