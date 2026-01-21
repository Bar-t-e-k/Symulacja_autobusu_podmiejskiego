#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include "common.h"
#include "ipc_utils.h"
#include "logs.h"

// Kolor logów
#define C_BUS  "\033[1;33m"

volatile sig_atomic_t wymuszony_odjazd = 0;

void sygnal_odjazd(int sig) {
    (void)sig;
    wymuszony_odjazd = 1;
}

// Pusty handler dla budzika - potrzebny, żeby przerwać czekanie na semaforze
void handler_alarm(int sig) {
    (void)sig;
}

// Logika autobusu/kierowcy
// Główna pętla życia procesu autobusu.
// Realizuje cykl: Czekanie na wjazd -> Załadunek -> Trasa -> Powrót.
void autobus_run(int id, int shmid, int semid) {
    signal(SIGUSR1, sygnal_odjazd);
    signal(SIGINT, SIG_IGN);
    
    SharedData* data = dolacz_pamiec(shmid);
    int P = data->cfg_P;
    int R = data->cfg_R;
    int T_ODJAZD = data->cfg_TP;
    odlacz_pamiec(data);
    
    srand(time(NULL) ^ (getpid() << 16));

    loguj(C_BUS, "[Autobus %d] Zgłasza gotowość na dworcu. Czekam na wjazd...\n", id);

    while (1) {
        // 1. Czekanie na wolne stanowisko
        // Autobus musi czekać w kolejce na dostęp do peronu (SEM_PRZYSTANEK).
        // Tylko jeden autobus może dokonywać załadunku w danej chwili.
        zablokuj_semafor(semid, SEM_PRZYSTANEK);

        // Po otrzymaniu dostępu, sprawdzamy czy dworzec nadal działa.
        // Jeśli w międzyczasie nastąpiło zamknięcie, zwalniamy zasoby i kończymy.
        zablokuj_semafor(semid, SEM_MUTEX);
        data = dolacz_pamiec(shmid);
        
        if (data->dworzec_otwarty == 0) { 
            odlacz_pamiec(data); 
            odblokuj_semafor(semid, SEM_MUTEX);
            odblokuj_semafor(semid, SEM_PRZYSTANEK);
            break; 
        }
        
        // 2. Zajęcie wolnego stanowiska
        data->autobus_obecny = 1; 
        data->pid_obecnego_autobusu = getpid();
        data->liczba_pasazerow = 0; 
        data->liczba_rowerow = 0;
        
        odlacz_pamiec(data);
        odblokuj_semafor(semid, SEM_MUTEX); 

        loguj(C_BUS, "[Autobus %d] Podstawiłem się. CZEKAM NA PASAŻERÓW (Czas: %ds)!\n", id, T_ODJAZD);

        // 3. Załadunek pasażerów
        // Autobus będzie spał, dopóki nie obudzi go pasażer lub minie czas
        signal(SIGALRM, handler_alarm);
        time_t czas_start = time(NULL);
        wymuszony_odjazd = 0;
        
        while(1) {
            // Sprawdzenie czy nie minął czas
            if (time(NULL) - czas_start >= T_ODJAZD) break;

            // Wyjazd na sygnał dyspozytora
            if (wymuszony_odjazd) break;

            zablokuj_semafor(semid, SEM_MUTEX);
            data = dolacz_pamiec(shmid);
            int liczba_oczekujacych = data->liczba_oczekujacych;
            int wolne_ludzie = P - data->liczba_pasazerow;
            odlacz_pamiec(data);
            odblokuj_semafor(semid, SEM_MUTEX);

            // Jeśli brak pasażerów lub miejsc -> uśpienie procesu
            if (liczba_oczekujacych == 0 || wolne_ludzie == 0) {
                int pozostalo = T_ODJAZD - (time(NULL) - czas_start);
                
                if (pozostalo <= 0) break;

                alarm(pozostalo);

                if (zablokuj_semafor_czekaj(semid, SEM_KTOS_CZEKA) == -1) break;

                alarm(0);
                continue;
            }

            // Konsumpcja sygnału w trybie nieblokującym.
            // Zapobiega to kumulacji "starych" dzwonków, gdy autobus obsługuje 
            // wielu pasażerów naraz w jednej pętli.
            struct sembuf sops = {SEM_KTOS_CZEKA, -1, IPC_NOWAIT};
            semop(semid, &sops, 1);
            
            // Analiza kolejek i decyzja kogo wpuścić
            zablokuj_semafor(semid, SEM_MUTEX);
            data = dolacz_pamiec(shmid);

            wolne_ludzie = P - data->liczba_pasazerow;
            int wolne_rowery = R - data->liczba_rowerow;
            
            int kogo_wolam = 0; // 0 - nikt, 1 - VIP, 2 - rower, 3 - zwykły/opiekun

            // VIP: Zawsze wchodzi pierwszy, omija kolejkę standardową
            if (wolne_ludzie > 0 && data->liczba_vip_oczekujacych > 0) {
                kogo_wolam = 1;
            }
            // Reszta: Sprawdzenie dostępność miejsc dla Rowerów i Zwykłych/Opiekunów.
            else if (wolne_ludzie > 0) {
                int sa_zwykli = (data->liczba_oczekujacych - data->liczba_rowerow_oczekujacych - data->liczba_vip_oczekujacych > 0);
                int sa_rowery = (data->liczba_rowerow_oczekujacych > 0 && wolne_rowery > 0);

                if (sa_zwykli && sa_rowery) {
                    if (rand() % 2 == 0) kogo_wolam = 2;
                    else kogo_wolam = 3;                
                }
                else if (sa_rowery) {
                    kogo_wolam = 2;
                }
                else if (sa_zwykli) {
                    kogo_wolam = 3;
                }
            }

            odlacz_pamiec(data);
            odblokuj_semafor(semid, SEM_MUTEX);
            
            if (kogo_wolam == 0) continue;

            // Fizyczne otwarcie drzwi (Budzenie procesu)
            if (kogo_wolam == 1) {
                odblokuj_semafor_bez_undo(semid, SEM_KOLEJKA_VIP);
            } else {
                if (kogo_wolam == 2) odblokuj_semafor_bez_undo(semid, SEM_DRZWI_ROW);
                else odblokuj_semafor_bez_undo(semid, SEM_DRZWI_PAS);
            }

            // Czekanie, aż pasażer podejmie decyzję (wsiądzie lub zrezygnuje).
            zablokuj_semafor_bez_undo(semid, SEM_WSIADL);
        }

        alarm(0);
        if (wymuszony_odjazd) {
            loguj(C_BUS, "[Autobus %d] Otrzymano nakaz natychmiastowego odjazdu!\n", id);
            wymuszony_odjazd = 0;
        } else {
            loguj(C_BUS, "[Autobus %d] Czas postoju minął.\n", id);
        }

        // 4. Odjazd
        zablokuj_semafor(semid, SEM_MUTEX);
        data = dolacz_pamiec(shmid);
        
        data->autobus_obecny = 0; 
        data->pid_obecnego_autobusu = 0;
        int p = data->liczba_pasazerow;
        int r = data->liczba_rowerow;
        
        odlacz_pamiec(data);
        odblokuj_semafor(semid, SEM_MUTEX);

        loguj(C_BUS, "[Autobus %d] ODJAZD z %d pasażerami (%d rowerów).\n", id, p, r);

        // Zwolnienie przystanku dla następnego autobusu
        odblokuj_semafor(semid, SEM_PRZYSTANEK);

        // 5. Trasa
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