#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include "common.h"
#include "ipc_utils.h"
#include "actors.h"

int main() {
    printf("Symulacja dworca..\n");

    // 1. Tworzenie zasobów
    int shmid = stworz_pamiec(sizeof(SharedData));
    int semid = stworz_semafor(LICZBA_SEMAFOROW);

    // 2. Inicjalizacja
    SharedData* data = dolacz_pamiec(shmid);
    data->liczba_pasazerow = 0;
    data->liczba_rowerow = 0;
    data->liczba_vip_oczekujacych = 0;
    data->autobus_obecny = 0;
    data->calkowita_liczba_pasazerow = 0;
    data->aktywne_autobusy = N;
    data->limit_pasazerow = LICZBA_PASAZEROW;
    data->koniec_symulacji = 0;
    odlacz_pamiec(data);

    ustaw_semafor(semid, SEM_MUTEX, 1); // MUTEX otwarty
    ustaw_semafor(semid, SEM_DRZWI_PAS, 1);  // Drzwi pasażerów
    ustaw_semafor(semid, SEM_DRZWI_ROW, 1);  // Drzwi rowerów

    // 3. Autobusy
    for (int b = 1; b <= N; b++) {
        if (fork() == 0) {
            autobus_run(b, shmid, semid);
        }
    }

    // 4. Pasażerowie
    srand(time(NULL));
    for (int i = 0; i < LICZBA_PASAZEROW; i++) {
        int los = rand() % 100;
        int typ = TYP_ZWYKLY;       

        if (los < 20) typ = TYP_VIP;         // 20% VIP
        else if (los < 50) typ = TYP_ROWER;  // 30% Rower
            
        if (fork() == 0) {
            pasazer_run(i + 1, shmid, semid, typ);
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