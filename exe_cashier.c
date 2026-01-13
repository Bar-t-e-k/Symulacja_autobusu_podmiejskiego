#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/msg.h>
#include <errno.h>
#include "common.h"
#include "ipc_utils.h"
#include "logs.h"

// Kolor
#define C_KASA  "\033[1;31m"

//Logika kasjera
void kasjer_run(int msgid) {
    BiletMsg msg;
    int rozmiar_danych = sizeof(BiletMsg) - sizeof(long);

    loguj(C_KASA, "[KASA] Otwieram okienko (PID: %d)...\n", getpid());

    while(1) {
        // Odebranie zapytania o bilet
        odbierz_komunikat(msgid, &msg, rozmiar_danych, KANAL_ZAPYTAN);

        loguj(C_KASA, "[KASA] Obsługuję pasażera PID %d...\n", msg.pid_nadawcy);
        
        usleep(50000); 

        // Odesłanie biletu
        msg.mtype = msg.pid_nadawcy; 
        wyslij_komunikat(msgid, &msg, rozmiar_danych);
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) return 1;
    int msgid = atoi(argv[1]);
    
    kasjer_run(msgid);
    return 0;
}