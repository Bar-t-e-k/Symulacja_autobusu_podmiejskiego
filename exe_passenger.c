#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <sys/msg.h>
#include <signal.h>
#include "common.h"
#include "ipc_utils.h"
#include "logs.h"

// Kolory logów
#define C_PAS   "\033[1;32m"
#define C_OPIE  "\033[1;34m"
#define C_VIP   "\033[1;35m"
#define C_ROW   "\033[1;36m"
#define C_BUS   "\033[1;33m"

// Mechanizmy synchronizacji wątku dziecka
pthread_mutex_t lock_dziecka = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_dziecka = PTHREAD_COND_INITIALIZER;
int koniec_podrozy = 0;

// Flaga wyjścia ustawiana przez SIGUSR2
volatile sig_atomic_t g_wyjscie = 0;

void handler_wyjscie(int sig) {
    (void)sig;
    g_wyjscie = 1;
}

// Funkcja wątku dziecka
// Symuluje obecność dziecka towarzyszącego opiekunowi.
// Wątek jest pasywny - po utworzeniu natychmiast zasypia na zmiennej warunkowej
// i budzi się dopiero, gdy Opiekun zasygnalizuje koniec.
void* watek_dziecka_fun(void* arg) {
    (void)arg;

    pthread_mutex_lock(&lock_dziecka);
    
    while (koniec_podrozy == 0) {
        pthread_cond_wait(&cond_dziecka, &lock_dziecka);
    }
    
    pthread_mutex_unlock(&lock_dziecka);

    return NULL;
}

// Bezpieczne wygaszenie wątku dziecka przed zakończeniem procesu
void zakoncz_watek_dziecka(pthread_t thread, int typ) {
    if (typ == TYP_OPIEKUN) {
        pthread_mutex_lock(&lock_dziecka);
        koniec_podrozy = 1;
        pthread_cond_signal(&cond_dziecka);
        pthread_mutex_unlock(&lock_dziecka);
        pthread_join(thread, NULL);
    }
}

// PROCEDURA WYJŚCIA
// Aktualizuje statystyki w pamięci współdzielonej, gdy pasażer musi wyjść
// z dworca przed wejściem do autobusu (np. przy zamknięciu dworca).
void raportuj_wyjscie(int shmid, int semid, int typ) {
    zablokuj_semafor(semid, SEM_MUTEX);
    SharedData* data = dolacz_pamiec(shmid);

    int liczba = (typ == TYP_OPIEKUN) ? 2 : 1;
    data->liczba_wyjsc += liczba;

    data->liczba_oczekujacych -= liczba;
    if (typ == TYP_VIP) data->liczba_vip_oczekujacych--;
    if (typ == TYP_ROWER) data->liczba_rowerow_oczekujacych--;

    odlacz_pamiec(data);
    odblokuj_semafor(semid, SEM_MUTEX);
}

// Główna logika procesu pasażera.
// Realizuje cykl: Wejście -> Kasa -> Przystanek -> Wsiadanie -> Koniec
void pasazer_run(int id, int shmid, int semid, int msgid_req, int msgid_res, int typ) {
    ustaw_sygnal(SIGUSR2, handler_wyjscie, 0);

    SharedData* data = dolacz_pamiec(shmid);
    srand(time(NULL) ^ (getpid()<<16)); 

    // Konfiguracja typu pasażera
    char* kolor = C_PAS;
    char* nazwa = "Zwykły";
    int moja_kolejka = SEM_DRZWI_PAS;

    pthread_t thread_dziecko;
    int id_dla_watku = id;

    if (typ == TYP_VIP) { 
        kolor = C_VIP; nazwa = "VIP"; moja_kolejka = SEM_KOLEJKA_VIP;
    }

    if (typ == TYP_ROWER) { 
        kolor = C_ROW; nazwa = "Rower"; moja_kolejka = SEM_DRZWI_ROW;
    }

    if (typ == TYP_OPIEKUN) { 
        kolor = C_OPIE; nazwa = "Opiekun"; moja_kolejka = SEM_DRZWI_PAS;
        if (pthread_create(&thread_dziecko, NULL, watek_dziecka_fun, &id_dla_watku) != 0) {
            loguj(kolor, "[Opiekun %d] Błąd tworzenia wątku dziecka!\n", id);
            exit(1);
        }
        loguj(kolor, "[Opiekun %d] Idę z dzieckiem (wątek utworzony).\n", id);
    }

    // 1. Wejście na dworzec
    // Sprawdzenie, czy dworzec jest otwarty. Jeśli nie - kończenie procesu.
    // Jeśli tak - pokazanie autobusowi, że ktoś czeka.
    zablokuj_semafor(semid, SEM_MUTEX);
    if (data->dworzec_otwarty == 0) {
        odlacz_pamiec(data);
        odblokuj_semafor(semid, SEM_MUTEX);
        
        zakoncz_watek_dziecka(thread_dziecko, typ);

        odblokuj_semafor_bez_undo(semid, SEM_LIMIT);
        exit(0);
    }
    data->liczba_oczekujacych++;
    if (typ == TYP_VIP) data->liczba_vip_oczekujacych++;
    if (typ == TYP_ROWER) data->liczba_rowerow_oczekujacych++;
    if (typ == TYP_OPIEKUN) data->liczba_oczekujacych++; // Dziecko
    odlacz_pamiec(data);
    odblokuj_semafor(semid, SEM_MUTEX);

    // 2. Kasa
    // Wysłanie zapytania do procesu Kasy i czekanie na odpowiedź.
    // VIP posiada bilet "stały", więc wysyła tylko informację (nie czeka w kolejce).
    int rozmiar = sizeof(BiletMsg) - sizeof(long);
    BiletMsg bilet;
    bilet.mtype = (typ == TYP_VIP) ? KANAL_KASA_VIP : KANAL_KASA;
    bilet.pid_nadawcy = getpid();
    bilet.typ_pasazera = typ;
    bilet.tid_dziecka = (typ == TYP_OPIEKUN) ? (unsigned long)thread_dziecko : 0;

    if (typ != TYP_VIP) {
        loguj(kolor, "[Pasażer %d (%s)] Idę do kasy (PID: %d).\n", id, nazwa, getpid());

        while (wyslij_komunikat(msgid_req, &bilet, rozmiar) == -1) {
            if (g_wyjscie) {
                raportuj_wyjscie(shmid, semid, typ);
                zakoncz_watek_dziecka(thread_dziecko, typ);
                odblokuj_semafor_bez_undo(semid, SEM_LIMIT);
                exit(0);
            }
        }

        while (odbierz_komunikat(msgid_res, &bilet, rozmiar, getpid()) == -1) {
            if (g_wyjscie) {
                raportuj_wyjscie(shmid, semid, typ);
                zakoncz_watek_dziecka(thread_dziecko, typ);
                odblokuj_semafor_bez_undo(semid, SEM_LIMIT);
                exit(0);
            }
        }
    } else {
        while (wyslij_komunikat(msgid_req, &bilet, rozmiar) == -1) {
             if (g_wyjscie) {
                raportuj_wyjscie(shmid, semid, typ);
                zakoncz_watek_dziecka(thread_dziecko, typ);
                odblokuj_semafor_bez_undo(semid, SEM_LIMIT);
                exit(0);
            }
        }

        loguj(kolor, "[Pasażer %d (VIP)] Mam karnet, omijam kolejkę do kasy. (PID: %d)\n", id, getpid());
    }

    loguj(kolor, "[Pasażer %d (%s)] Przychodzę na przystanek.\n", id, nazwa);

    odblokuj_semafor_bez_undo(semid, SEM_KTOS_CZEKA);

    // 3. Wsiadanie
    // Pasażer zasypia na semaforze. Budzi go kierowca.
    // Po obudzeniu pasażer sprawdza, czy na pewno się zmieści.
    // Jeśli nie (np. Opiekun potrzebuje 2 miejsc, a jest 1) - wraca spać.
    int wszedlem = 0;
    while (!wszedlem) {
        if (zablokuj_semafor_czekaj(semid, moja_kolejka) == -1) {
            if (g_wyjscie) {
                raportuj_wyjscie(shmid, semid, typ);
                zakoncz_watek_dziecka(thread_dziecko, typ);
                odblokuj_semafor_bez_undo(semid, SEM_LIMIT);
                exit(0);
            }
            continue; 
        }
        
        zablokuj_semafor(semid, SEM_MUTEX);
        data = dolacz_pamiec(shmid);

        // Sprawdzenie stanu dworca
        if (data->dworzec_otwarty == 0 || g_wyjscie) {
            odlacz_pamiec(data);
            odblokuj_semafor(semid, SEM_MUTEX);
            
            raportuj_wyjscie(shmid, semid, typ);
            zakoncz_watek_dziecka(thread_dziecko, typ);
            odblokuj_semafor_bez_undo(semid, SEM_LIMIT);
            exit(0);
        }

        // Sprawdzenie czy jest odpowiednia ilość miejsc
        int miejsca_potrzebne = (typ == TYP_OPIEKUN) ? 2 : 1;
        int wolne = data->cfg_P - data->liczba_pasazerow;
        int wolne_rowery = data->cfg_R - data->liczba_rowerow;

        if (miejsca_potrzebne <= wolne && (typ != TYP_ROWER || wolne_rowery > 0)) {
            int rower_potrzebny = (typ == TYP_ROWER) ? 1 : 0;

            data->liczba_pasazerow += miejsca_potrzebne;
            data->liczba_rowerow += rower_potrzebny;

            data->calkowita_liczba_pasazerow += miejsca_potrzebne;
            
            data->liczba_oczekujacych -= miejsca_potrzebne;
            if (typ == TYP_VIP) data->liczba_vip_oczekujacych--;
            if (typ == TYP_ROWER) data->liczba_rowerow_oczekujacych--;

            loguj(kolor, "[Pasażer %d (%s)] Okazuję bilet i wsiadam! (Stan: %d/%d, Rowery: %d/%d)\n", id, nazwa, data->liczba_pasazerow, data->cfg_P,
            data->liczba_rowerow, data->cfg_R);
            if (typ == TYP_OPIEKUN) loguj(kolor, "[Opiekun %d] Wprowadzam dziecko do autobusu.\n", id);

            wszedlem = 1;
        }
        
        odlacz_pamiec(data); 
        odblokuj_semafor(semid, SEM_MUTEX);

        // Poinformawanie kierowcy, że decyzja podjęta
        odblokuj_semafor_bez_undo(semid, SEM_WSIADL);
    }

    // Koniec podróży
    zakoncz_watek_dziecka(thread_dziecko, typ);

    odblokuj_semafor_bez_undo(semid, SEM_LIMIT);
    exit(0);
}

int main(int argc, char* argv[]) {
    if (argc < 7) return 1;
    pasazer_run(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), atoi(argv[5]), atoi(argv[6]));
    return 0;
}