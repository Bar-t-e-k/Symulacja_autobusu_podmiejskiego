#ifndef COMMON_H
#define COMMON_H

#include <sys/types.h>

// Parametry symulacji
#define P 10          // Pojemność autobusu
#define R 4           // Miejsca na rowery
#define N 3           // Liczba autobusów
#define T_ODJAZD 5    // Czas oczekiwania (dla testów krótki)

// Klucze do IPC
#define SHM_KEY_PATH "."
#define SHM_KEY_ID 'A'
#define SEM_KEY_ID 'B'

// --- SEMAFORY (Indeksy w tablicy) ---
#define SEM_MUTEX 0   // Chroni dostęp do pamięci dzielonej
#define LICZBA_SEMAFOROW 1 // Na razie jeden do ochrony danych

// Struktura w Pamięci Dzielonej
typedef struct {
    int liczba_pasazerow;   // Ile osób jest w środku
    int liczba_rowerow;     // Ile rowerów jest w środku
    int autobus_obecny;     // 1 = jest autobus, 0 = nie ma
    int odjazd_wymuszony;   // Flaga od dyspozytora
    
    // Ile osób obsłużono łącznie
    int calkowita_liczba_pasazerow;
} SharedData;

#endif