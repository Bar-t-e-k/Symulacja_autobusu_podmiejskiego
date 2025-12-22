#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

// Stałe (docelowo przeniesiemy do common.h)
#define LICZBA_AUTOBUSOW 3

void proces_autobus(int id) {
    printf("[AUTOBUS %d] Uruchomiony (PID: %d)\n", id, getpid());
    // Symulacja pracy
    sleep(2);
    printf("[AUTOBUS %d] Konczy prace\n", id);
    exit(0);
}

int main() {
    printf("[MAIN] Start symulacji dworca. PID: %d\n", getpid());

    // Tworzenie procesów autobusów
    for (int i = 0; i < LICZBA_AUTOBUSOW; i++) {
        pid_t pid = fork();

        if (pid == -1) {
            perror("Blad fork");
            exit(1);
        } else if (pid == 0) {
            // To jest kod PROCESU POTOMNEGO (Autobusu)
            proces_autobus(i + 1);
            // Proces potomny NIE MOŻE iść dalej w pętli main, musi się zakończyć
            exit(0); 
        }
    }

    // Kod PROCESU MACIERZYSTEGO (Dworca/Głównego)
    // Czekamy, aż wszystkie dzieci zakończą pracę (żeby nie było zombie)
    for (int i = 0; i < LICZBA_AUTOBUSOW; i++) {
        wait(NULL);
    }

    printf("[MAIN] Wszystkie autobusy zjechaly. Koniec symulacji.\n");
    return 0;
}