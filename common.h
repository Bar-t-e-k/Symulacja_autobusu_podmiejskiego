#ifndef COMMON_H
#define COMMON_H

#include <sys/types.h>

// KLUCZE DO IPC
#define SHM_KEY_PATH "."
#define SHM_KEY_ID 'A'
#define SEM_KEY_ID 'B'
#define MSG_KEY_ID_REQ 'C'
#define MSG_KEY_ID_RES 'D'

// SEMAFORY
#define SEM_MUTEX 0  // Semafor binarny - chroni dostęp do pamięci dzielonej
#define SEM_DRZWI_PAS  1  // Blokuje pasażerów zwykłych i opiekunów przed wejściem
#define SEM_DRZWI_ROW  2  // Dedykowana blokada dla pasażerów z rowerami
#define SEM_KOLEJKA_VIP 3 // Priorytetowe wejście dla VIP-ów 
#define SEM_WSIADL  4 // Pasażer potwierdza zakończenie procesu wsiadania
#define SEM_PRZYSTANEK  5  // Zarządza wjazdem na peron
#define SEM_LIMIT  6  // Bariera wejściowa na dworzec
#define SEM_KTOS_CZEKA 7 // Budzi uśpiony autobus, gdy na przystanku pojawi się pasażer
#define LICZBA_SEMAFOROW 8

// TYPY PASAŻERÓW
#define TYP_ZWYKLY 0
#define TYP_VIP    1
#define TYP_ROWER  2
#define TYP_OPIEKUN 3

// KANAŁY KOMUNIKACJI
#define KANAL_KASA_VIP 1 // Typ komunikatu priorytetowego
#define KANAL_KASA 2 // Typ komunikatu zwykłego

// PAMIEĆ DZIELONA
typedef struct {
    // Konfiguracja z pliku
    int cfg_P; // Pojemność autobusu
    int cfg_R; // Miejsca na rowery
    int cfg_N; // Liczba autobusów
    int cfg_TP; // Czas postoju (w sekundach)
    
    // Stan symulacji
    int liczba_pasazerow; // Ile osób jest w środku autobusu
    int liczba_rowerow; // Ile rowerów jest w środku autobusu
    int autobus_obecny; // 1 = jest autobus, 0 = nie ma autobusu
    pid_t pid_obecnego_autobusu; // Do adresowania sygnałów SIGUSR1

    int calkowita_liczba_pasazerow; // Ile osób obsłużono łącznie
    int liczba_wyjsc; // Ile osób musiało opuścić dworzec ze względu na zamknięcie
    int aktywne_autobusy; // Ile autobusów jeszcze pracuje
    int dworzec_otwarty; // Flaga: 1 = Otwarty, 0 = Zamknięty

    // Liczniki osób fizycznie stojących na przystanku
    int liczba_oczekujacych; // Licznik ogólny
    int liczba_vip_oczekujacych;
    int liczba_rowerow_oczekujacych;
} SharedData;

// KOLEJKA KOMUNIKATÓW
typedef struct {
    long mtype; // typ komunikatu
    int pid_nadawcy; // PID pasażera, aby Kasjer wiedział komu odpisać
    int typ_pasazera; // Do celów logowania statystyk w Kasie
    unsigned long tid_dziecka; // Przechowuje ID wątku dziecka (dla Opiekunów)
} BiletMsg;

#endif