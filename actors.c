#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include "common.h"
#include "ipc_utils.h"

// Kolory dla czytelności
#define C_BUS "\033[1;33m" // Żółty
#define C_PAS "\033[1;32m" // Zielony
#define C_RST "\033[0m"

void pasazer_run(int id, int shmid, int semid) {
    SharedData* data = dolacz_pamiec(shmid);
    srand(time(NULL) ^ (getpid()<<16)); // Unikalne ziarno losowości

    printf(C_PAS "[Pasażer %d] Przychodzi na przystanek.\n" C_RST, id);

    int wszedlem = 0;
    while (!wszedlem) {
        // 1. Sekcja Krytyczna - sprawdzenie stanu dworca
        zablokuj_semafor(semid, SEM_MUTEX);

        if (data->autobus_obecny == 1 && data->liczba_pasazerow < P) {
            // JEST MIEJSCE!
            data->liczba_pasazerow++;
            data->calkowita_liczba_pasazerow++;
            printf(C_PAS "[Pasażer %d] WSIADŁ! (Miejsc zajętych: %d/%d)\n" C_RST, 
                   id, data->liczba_pasazerow, P);
            wszedlem = 1;
        } else {
            // BRAK MIEJSCA LUB AUTOBUSU
            printf("[Pasażer %d] Czekam... (Bus: %d, Miejsc: %d)\n", id, data->autobus_obecny, data->liczba_pasazerow);
        }

        odblokuj_semafor(semid, SEM_MUTEX); // Koniec sekcji krytycznej

        if (!wszedlem) {
            usleep(100000); // Czeka 0.1s przed ponowną próbą
        }
    }

    odlacz_pamiec(data);
    exit(0);
}

void autobus_run(int id, int shmid, int semid) {
    SharedData* data = dolacz_pamiec(shmid);
    
    // Pętla pracy autobusu
    for (int kurs = 1; kurs <= 3; kurs++) {
        printf(C_BUS "[Autobus %d] Podjeżdża na przystanek (Kurs %d).\n" C_RST, id, kurs);

        // 1. Podstawienie autobusu
        zablokuj_semafor(semid, SEM_MUTEX);
        data->autobus_obecny = 1;
        data->liczba_pasazerow = 0; // Pusty autobus
        data->liczba_rowerow = 0;
        odblokuj_semafor(semid, SEM_MUTEX);

        // 2. Czekanie na pasażerów
        printf(C_BUS "[Autobus %d] Otwiera drzwi na %d sekund...\n" C_RST, id, T_ODJAZD);
        sleep(T_ODJAZD);

        // 3. Odjazd
        zablokuj_semafor(semid, SEM_MUTEX);
        data->autobus_obecny = 0; // Zamyka drzwi
        int zabranych = data->liczba_pasazerow;
        odblokuj_semafor(semid, SEM_MUTEX);

        printf(C_BUS "[Autobus %d] ODJAZD z %d pasażerami! Trasa...\n" C_RST, id, zabranych);
        
        // Symulacja jazdy
        sleep(2); 
        printf(C_BUS "[Autobus %d] Wrócił z trasy.\n" C_RST, id);
    }

    odlacz_pamiec(data);
    exit(0);
}