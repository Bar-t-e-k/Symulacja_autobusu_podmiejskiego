#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include "common.h"
#include "ipc_utils.h"
#include "logs.h"

// Kolor
#define C_BUS  "\033[1;33m"

volatile sig_atomic_t wymuszony_odjazd = 0;

void sygnal_odjazd(int sig) {
    (void)sig;
    wymuszony_odjazd = 1;
}

// Logika autobusu/kierowcy
void autobus_run(int id, int shmid, int semid) {
    signal(SIGUSR1, sygnal_odjazd);
    signal(SIGINT, SIG_IGN);
    
    SharedData* data = dolacz_pamiec(shmid);
    int P = data->cfg_P;
    int T_ODJAZD = data->cfg_TP;
    odlacz_pamiec(data);
    
    srand(time(NULL) ^ (getpid() << 16));

    loguj(C_BUS, "[Autobus %d] Zgłasza gotowość na dworcu. Czekam na wjazd...\n", id);

    while (1) {
        int czy_koniec_pracy = 0;

        // 1. Czekanie na wolne stanowisko
        while(1) {
             zablokuj_semafor(semid, SEM_MUTEX);
             data = dolacz_pamiec(shmid);
             
             if (data->dworzec_otwarty == 0) { 
                 odlacz_pamiec(data); 
                 odblokuj_semafor(semid, SEM_MUTEX); 
                 czy_koniec_pracy = 1; 
                 break; 
             }
             
             if (data->autobus_obecny == 0) {
                 data->autobus_obecny = 1; 
                 data->pid_obecnego_autobusu = getpid();
                 data->liczba_pasazerow = 0; 
                 data->liczba_rowerow = 0;
                 
                 odlacz_pamiec(data);
                 odblokuj_semafor(semid, SEM_MUTEX); 
                 break;
             }
             
             odlacz_pamiec(data);
             odblokuj_semafor(semid, SEM_MUTEX); 
             usleep(200000); 
        }

        if (czy_koniec_pracy) break;

        loguj(C_BUS, "[Autobus %d] Podstawiłem się. CZEKAM NA PASAŻERÓW (Czas: %ds)!\n", id, T_ODJAZD);

        // 2. Załadunek pasażerów
        time_t start = time(NULL);
        wymuszony_odjazd = 0;
        int byl_komunikat_pelny = 0;
        
        while(1) {
            // Opcja 1: Czas minął
            if (time(NULL) - start >= T_ODJAZD) {
                loguj(C_BUS, "[Autobus %d] Czas postoju minął.\n", id);
                break;
            }
            
            // Opcja 2: Sygnał
            if (wymuszony_odjazd) {
                loguj(C_BUS, "[Autobus %d] Otrzymano nakaz natychmiastowego odjazdu!\n", id);
                wymuszony_odjazd = 0;
                break;
            }

            // Sprawdzenie czy autobus pełny i wypisanie komunikatu
            if (!byl_komunikat_pelny) {
                zablokuj_semafor(semid, SEM_MUTEX);
                data = dolacz_pamiec(shmid);
                int aktualna_l_pas = data->liczba_pasazerow;
                odlacz_pamiec(data);
                odblokuj_semafor(semid, SEM_MUTEX);

                if (aktualna_l_pas >= P) {
                    loguj(C_BUS, "[Autobus %d] Komplet pasażerów (%d/%d). Czekam na godzinę odjazdu...\n", id, aktualna_l_pas, P);
                    byl_komunikat_pelny = 1;
                }
            }
            usleep(100000); // 0.1s
        }

        // 3. Odjazd
        zablokuj_semafor(semid, SEM_MUTEX);
        data = dolacz_pamiec(shmid);
        
        data->autobus_obecny = 0; 
        data->pid_obecnego_autobusu = 0;
        int p = data->liczba_pasazerow;
        int r = data->liczba_rowerow;
        
        odlacz_pamiec(data);
        odblokuj_semafor(semid, SEM_MUTEX);

        loguj(C_BUS, "[Autobus %d] ODJAZD z %d pasażerami (%d rowerów).\n", id, p, r);

        // 4. Trasa
        int czas_trasy = 5 + (rand() % 11); 
        sleep(czas_trasy);
        loguj(C_BUS, "[Autobus %d] WRÓCIŁ Z TRASY po %d s.\n", id, czas_trasy);
        
        // Sprawdzenie czy kontynuować po powrocie
        zablokuj_semafor(semid, SEM_MUTEX);
        data = dolacz_pamiec(shmid);
        if (data->dworzec_otwarty == 0) {
            odlacz_pamiec(data);
            odblokuj_semafor(semid, SEM_MUTEX);
            break; 
        }
        odlacz_pamiec(data);
        odblokuj_semafor(semid, SEM_MUTEX);
    }

    // Koniec pracy
    zablokuj_semafor(semid, SEM_MUTEX);
    data = dolacz_pamiec(shmid);
    data->aktywne_autobusy--;
    loguj(C_BUS, "[Autobus %d] Zjazd do zajezdni (Koniec pracy).\n", id);
    odlacz_pamiec(data);
    odblokuj_semafor(semid, SEM_MUTEX);
    
    exit(0);
}

int main(int argc, char* argv[]) {
    if (argc < 4) return 1;
    autobus_run(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]));
    return 0;
}