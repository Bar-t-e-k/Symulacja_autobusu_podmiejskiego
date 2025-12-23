#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include "common.h"
#include "ipc_utils.h"
#include "actors.h"

#define LICZBA_PASAZEROW 15 // Więcej niż miejsc (P=5), żeby przetestować tłok

int main() {
    printf("Symulacja dworca..\n");

    // 1. Tworzenie zasobów
    int shmid = stworz_pamiec(sizeof(SharedData));
    int semid = stworz_semafor(LICZBA_SEMAFOROW);

    // 2. Inicjalizacja
    SharedData* data = dolacz_pamiec(shmid);
    data->liczba_pasazerow = 0;
    data->liczba_rowerow = 0;
    data->autobus_obecny = 0;
    ustaw_semafor(semid, SEM_MUTEX, 1); // MUTEX otwarty
    odlacz_pamiec(data);

    // 3. Autobus
    if (fork() == 0) {
        autobus_run(1, shmid, semid);
    }

    // 4. Pasażerowie
    for (int i = 0; i < LICZBA_PASAZEROW; i++) {
        if (fork() == 0) {
            pasazer_run(i + 1, shmid, semid);
        }
        usleep(200000); // Nowy pasażer co 0.2s
    }

    // 5. Czekanie na zakończenie wszystkich (na razie uproszczone)
    while (wait(NULL) > 0);
    
    // 6. Raport końcowy
    data = dolacz_pamiec(shmid);
    printf("\n--- RAPORT KOŃCOWY ---\n");
    printf("Łącznie obsłużono pasażerów: %d\n", data->calkowita_liczba_pasazerow);

    // 7. Sprzątanie
    odlacz_pamiec(data);
    usun_pamiec(shmid);
    usun_semafor(semid);

    return 0;
}