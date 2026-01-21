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
#define SEM_DRZWI_PAS  1  // drzwi dla pasażerów
#define SEM_DRZWI_ROW  2  // drzwi dla rowerów 
#define SEM_KOLEJKA_VIP 3 // kolejka priorytetowa dla VIPA 
#define SEM_WSIADL  4 // synchronizacja decyzji pasażera
#define SEM_PRZYSTANEK  5  // wjazd na przystanek
#define SEM_LIMIT  6  // ograniczenie generowania pasażerów 
#define SEM_KTOS_CZEKA 7 // budzenie kierowcy
#define LICZBA_SEMAFOROW 8

// TYPY PASAŻERÓW
#define TYP_ZWYKLY 0
#define TYP_VIP    1
#define TYP_ROWER  2
#define TYP_OPIEKUN 3

// TYPY KOMUNIKATÓW
#define KANAL_KASA_VIP 1 // Typ komunikatu priorytetowego
#define KANAL_KASA 2 // Typ komunikatu zwykłego

// PAMIEĆ DZIELONA
typedef struct {
    // Konfiguracja z pliku
    int cfg_P; // Pojemność autobusu
    int cfg_R; // Miejsca na rowery
    int cfg_N; // Liczba autobusów
    int cfg_TP; // Czas postoju
    
    // Stan symulacji
    int liczba_pasazerow;   // Ile osób jest w środku
    int liczba_rowerow;     // Ile rowerów jest w środku
    int autobus_obecny;     // 1 = jest autobus, 0 = nie ma autobusu
    pid_t pid_obecnego_autobusu;

    int calkowita_liczba_pasazerow; // Ile osób obsłużono łącznie
    int aktywne_autobusy;
    int dworzec_otwarty;   // Flaga: 1 = Otwarte, 0 = Zamknięte

    int liczba_oczekujacych;
    int liczba_vip_oczekujacych;
    int liczba_rowerow_oczekujacych;
} SharedData;

// KOLEJKA KOMUNIKATÓW
typedef struct {
    long mtype; // typ komunikatu
    int pid_nadawcy;
    int typ_pasazera;
    unsigned long tid_dziecka; // 0 jeśli pasażer jest sam
} BiletMsg;

#endif