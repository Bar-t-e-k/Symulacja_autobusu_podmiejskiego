#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <sys/msg.h>
#include "common.h"
#include "ipc_utils.h"
#include "logs.h"

// Kolory
#define C_PAS   "\033[1;32m"
#define C_OPIE  "\033[1;34m"
#define C_VIP   "\033[1;35m"
#define C_ROW   "\033[1;36m"
#define C_BUS   "\033[1;33m"

// Mechanizmy synchronizacji wątku
pthread_mutex_t lock_dziecka = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_dziecka = PTHREAD_COND_INITIALIZER;
int koniec_podrozy = 0;

// Funkcja wątku dziecka
void* watek_dziecka_fun(void* arg) {
    (void)arg;

    pthread_mutex_lock(&lock_dziecka);
    
    // Dziecko czeka na sygnał
    // Wątek budzi się dopiero, gdy otrzyma sygnał i warunek pętli będzie fałszywy
    while (koniec_podrozy == 0) {
        pthread_cond_wait(&cond_dziecka, &lock_dziecka);
    }
    
    pthread_mutex_unlock(&lock_dziecka);

    return NULL;
}

// Logika pasażera
void pasazer_run(int id, int shmid, int semid, int msgid_req, int msgid_res, int typ) {
    SharedData* data = dolacz_pamiec(shmid);
    srand(time(NULL) ^ (getpid()<<16)); 

    char* kolor = C_PAS;
    char* nazwa = "Zwykły";
    int moje_drzwi = SEM_DRZWI_PAS;

    pthread_t thread_dziecko;
    int id_dla_watku = id;

    if (typ == TYP_VIP) { 
        kolor = C_VIP; nazwa = "VIP"; moje_drzwi = SEM_DRZWI_PAS;
    }

    if (typ == TYP_ROWER) { 
        kolor = C_ROW; nazwa = "Rower"; moje_drzwi = SEM_DRZWI_ROW;
    }

    if (typ == TYP_OPIEKUN) { 
        kolor = C_OPIE; nazwa = "Opiekun"; moje_drzwi = SEM_DRZWI_PAS;
        if (pthread_create(&thread_dziecko, NULL, watek_dziecka_fun, &id_dla_watku) != 0) {
            loguj(kolor, "[Opiekun %d] Błąd tworzenia wątku dziecka!\n", id);
            exit(1);
        }
        loguj(kolor, "[Opiekun %d] Idę z dzieckiem (wątek utworzony).\n", id);
    }

    // 1. Wejście na dworzec
    zablokuj_semafor(semid, SEM_MUTEX);
    if (data->dworzec_otwarty == 0) {
        loguj(kolor, "[Pasażer %d] Dworzec zamknięty! Nie wchodzę.\n", id);
        odblokuj_semafor(semid, SEM_MUTEX);
        odlacz_pamiec(data);
        
        if (typ == TYP_OPIEKUN) {
            pthread_mutex_lock(&lock_dziecka);
            koniec_podrozy = 1;
            pthread_cond_signal(&cond_dziecka); 
            pthread_mutex_unlock(&lock_dziecka);
            pthread_join(thread_dziecko, NULL);
        }

        exit(0);
    }
    data->liczba_oczekujacych++;
    if (typ == TYP_VIP) data->liczba_vip_oczekujacych++;
    if (typ == TYP_OPIEKUN) data->liczba_oczekujacych++; // Dziecko
    odblokuj_semafor(semid, SEM_MUTEX);

    // 2. Kasa
    int rozmiar = sizeof(BiletMsg) - sizeof(long);

    if (typ != TYP_VIP) {
        loguj(kolor, "[Pasażer %d (%s)] Idę do kasy (PID: %d).\n", id, nazwa, getpid());
        
        BiletMsg bilet;
        bilet.mtype = KANAL_KASA;
        bilet.pid_nadawcy = getpid();
        bilet.typ_pasazera = typ;

        if (typ == TYP_OPIEKUN) {
            bilet.tid_dziecka = (unsigned long)thread_dziecko;
        } else {
            bilet.tid_dziecka = 0;
        }

        wyslij_komunikat(msgid_req, &bilet, rozmiar);
        odbierz_komunikat(msgid_res, &bilet, rozmiar, getpid());
    } else {
        BiletMsg bilet;
        bilet.mtype = KANAL_KASA;
        bilet.pid_nadawcy = getpid();
        bilet.typ_pasazera = typ;

        wyslij_komunikat(msgid_req, &bilet, rozmiar);
        loguj(kolor, "[Pasażer %d (VIP)] Mam karnet, omijam kolejkę do kasy. (PID: %d)\n", id, getpid());
    }

    loguj(kolor, "[Pasażer %d (%s)] Przychodzę na przystanek.\n", id, nazwa);

    // 3. Wsiadanie
    int wszedlem = 0;
    while (!wszedlem) {
        // Blokada drzwi (VIP omija kolejkę)
        if (typ != TYP_VIP) zablokuj_semafor(semid, moje_drzwi);
        
        zablokuj_semafor(semid, SEM_MUTEX);
        data = dolacz_pamiec(shmid);

        if (data->dworzec_otwarty == 0) {
            loguj(kolor, "[Pasażer %d] Dworzec zamknięty! Wychodzę.\n", id);
            data->liczba_oczekujacych--;
            if (typ == TYP_OPIEKUN) data->liczba_oczekujacych--;
            if (typ == TYP_VIP) data->liczba_vip_oczekujacych--;

            odlacz_pamiec(data);
            odblokuj_semafor(semid, SEM_MUTEX);
            if (typ != TYP_VIP) odblokuj_semafor(semid, moje_drzwi);

            if (typ == TYP_OPIEKUN) {
                pthread_mutex_lock(&lock_dziecka);
                koniec_podrozy = 1;
                pthread_cond_signal(&cond_dziecka);
                pthread_mutex_unlock(&lock_dziecka);
                pthread_join(thread_dziecko, NULL);
            }
            exit(0);
        }

        // Sprawdzanie priorytetu VIPA
        if (typ != TYP_VIP && data->liczba_vip_oczekujacych > 0) {
            odlacz_pamiec(data); 
            odblokuj_semafor(semid, SEM_MUTEX);
            if (typ != TYP_VIP) odblokuj_semafor(semid, moje_drzwi);
            usleep(100000); 
            continue;
        }

        int stan_ludzi = 0;
        int stan_rowerow = 0;
        int P = data->cfg_P;
        int R = data->cfg_R;

        if (data->autobus_obecny == 1) {
            int miejsca_potrzebne = (typ == TYP_OPIEKUN) ? 2 : 1;
            int rower_potrzebny = (typ == TYP_ROWER) ? 1 : 0;
            
            int czy_zmieszcze_sie = 1;
            if (data->liczba_pasazerow + miejsca_potrzebne > P) czy_zmieszcze_sie = 0;
            if (data->liczba_rowerow + rower_potrzebny > R) czy_zmieszcze_sie = 0;

            if (czy_zmieszcze_sie) {
                data->liczba_pasazerow += miejsca_potrzebne;
                data->liczba_rowerow += rower_potrzebny;

                data->calkowita_liczba_pasazerow += miejsca_potrzebne;
                
                data->liczba_oczekujacych -= miejsca_potrzebne;
                if (typ == TYP_VIP) data->liczba_vip_oczekujacych--;

                stan_ludzi = data->liczba_pasazerow;
                stan_rowerow = data->liczba_rowerow;

                loguj(kolor, "[Pasażer %d (%s)] Okazuję bilet i wsiadam!\n", id, nazwa);
                if (typ == TYP_OPIEKUN) loguj(kolor, "[Opiekun %d] Wprowadzam dziecko do autobusu.\n", id);
                loguj(C_BUS, "   -> [Stan Autobusu] Pasażerów: %d/%d (Rowerów: %d/%d)\n", stan_ludzi, P, stan_rowerow, R);
                
                wszedlem = 1;
            }
        }
        
        odlacz_pamiec(data); 
        odblokuj_semafor(semid, SEM_MUTEX);

        if (!wszedlem) {
            if (typ != TYP_VIP) odblokuj_semafor(semid, moje_drzwi); // Zwolnienie drzwi dla innych
            usleep(500000);
        } else {
            if (typ != TYP_VIP) odblokuj_semafor(semid, moje_drzwi); // Zwolnienie drzwi dla innych, bo już jest w środku
        }
    }

    if (typ == TYP_OPIEKUN) {
        pthread_mutex_lock(&lock_dziecka);
        koniec_podrozy = 1;
        pthread_cond_signal(&cond_dziecka);
        pthread_mutex_unlock(&lock_dziecka);
        pthread_join(thread_dziecko, NULL);
    }
    exit(0);
}

int main(int argc, char* argv[]) {
    if (argc < 7) return 1;
    pasazer_run(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), atoi(argv[5]), atoi(argv[6]));
    return 0;
}