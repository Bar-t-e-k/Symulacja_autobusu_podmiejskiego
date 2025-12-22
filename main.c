#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include "common.h"
#include "ipc_utils.h"

int main() {
    printf("[MAIN] Inicjalizacja zasobow IPC...\n");

    // 1. Tworzenie zasobów
    int shmid = stworz_pamiec(sizeof(SharedData));
    int semid = stworz_semafor();

    // 2. Inicjalizacja
    SharedData* data = dolacz_pamiec(shmid);
    data->liczba_pasazerow = 0;
    data->liczba_rowerow = 0;
    data->autobus_obecny = 0;
    
    ustaw_semafor(semid, 1); // Sem = 1 (Otwarty)

    printf("[MAIN] Pamięć ID: %d, Semafor ID: %d\n", shmid, semid);

    // 3. Test Fork
    pid_t pid = fork();
    if (pid == 0) {
        // --- PROCES POTOMNY (np. Pasażer) ---
        SharedData* child_data = dolacz_pamiec(shmid);
        
        printf("[DZIECKO] Probuje wejsc do sekcji krytycznej...\n");
        zablokuj_semafor(semid);
        
        printf("[DZIECKO] Jestem w sekcji! Zwiekszam licznik.\n");
        child_data->liczba_pasazerow += 5;
        sleep(2); // Symulacja pracy, zeby zobaczyc czy blokuje
        
        odblokuj_semafor(semid);
        printf("[DZIECKO] Wyjscie.\n");
        
        odlacz_pamiec(child_data);
        exit(0);
    }

    // --- PROCES GLOWNY ---
    // Czekamy chwile, zeby dziecko na pewno weszlo pierwsze
    sleep(1); 
    
    printf("[MAIN] Probuje odczytac dane...\n");
    zablokuj_semafor(semid); // To powinno poczekac, az dziecko skonczy!
    printf("[MAIN] Odczytano liczbe pasazerow: %d\n", data->liczba_pasazerow);
    odblokuj_semafor(semid);

    wait(NULL); // Czekaj na dziecko

    // 4. Sprzatanie
    printf("[MAIN] Czyszczenie IPC...\n");
    odlacz_pamiec(data);
    usun_pamiec(shmid);
    usun_semafor(semid);

    return 0;
}