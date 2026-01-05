#ifndef COMMON_H
#define COMMON_H

#include <sys/types.h>

// Parametry symulacji
#define P 10          // Pojemność autobusu
#define R 4           // Miejsca na rowery
#define N 3           // Liczba autobusów
#define T_ODJAZD 5   // Czas oczekiwania
#define CZAS_PRZEJAZDU 10 // Czas przejazdu
#define LICZBA_PASAZEROW 20 // Łączna liczba pasażerów do obsłużenia

// Klucze do IPC
#define SHM_KEY_PATH "."
#define SHM_KEY_ID 'A'
#define SEM_KEY_ID 'B'
#define MSG_KEY_ID 'C'

// SEMAFORY
#define SEM_MUTEX 0   // Chroni dostęp do pamięci dzielonej
#define SEM_DRZWI_PAS  1  // drzwi dla pasażerów
#define SEM_DRZWI_ROW  2  // drzwi dla rowerów 

#define LICZBA_SEMAFOROW 3

// TYPY PASAŻERÓW
#define TYP_ZWYKLY 0
#define TYP_VIP    1
#define TYP_ROWER  2

// Pamięć Dzielona
typedef struct {
    int liczba_pasazerow;   // Ile osób jest w środku
    int liczba_rowerow;     // Ile rowerów jest w środku
    int autobus_obecny;     // 1 = jest autobus, 0 = nie ma autobusu
    
    int liczba_vip_oczekujacych;

    int limit_pasazerow;
    int pasazerowie_obsluzeni;
    int aktywne_autobusy;
    
    pid_t pid_obecnego_autobusu;
    int dworzec_otwarty;   // Flaga: 1 = Otwarte, 0 = Zamknięte

    // Ile osób obsłużono łącznie
    int calkowita_liczba_pasazerow;
} SharedData;

// Kolejka komunikatów
typedef struct {
    long mtype;
    int pid_nadawcy;
    int typ_pasazera;
} BiletMsg;

#define KANAL_ZAPYTAN 1 // Kanał dla pasażerów pytających o bilet

#endif