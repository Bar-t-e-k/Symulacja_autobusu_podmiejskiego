#ifndef COMMON_H
#define COMMON_H

#include <sys/types.h>

// Parametry symulacji
#define P 10          // Pojemność autobusu
#define R 4           // Miejsca na rowery
#define N 3           // Liczba autobusów
#define T_ODJAZD 5    // Czas oczekiwania (dla testów krótki)

// Klucze do IPC (zamiast hardcodowania liczb)
#define SHM_KEY_PATH "."
#define SHM_KEY_ID 'A'
#define SEM_KEY_ID 'B'

// Struktura w Pamięci Dzielonej
// To jest to, co "widzą" wszystkie procesy naraz
typedef struct {
    int liczba_pasazerow;   // Ile osób jest w środku
    int liczba_rowerow;     // Ile rowerów jest w środku
    int autobus_obecny;     // 1 = jest autobus, 0 = nie ma
    int odjazd_wymuszony;   // Flaga od dyspozytora
    
    // Możesz tu dodać liczniki statystyk, np. ile osób obsłużono łącznie
    int calkowita_liczba_pasazerow;
} SharedData;

#endif