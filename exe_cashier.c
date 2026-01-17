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
void kasjer_run(int msgid_req, int msgid_res) {
    BiletMsg msg;
    int rozmiar_danych = sizeof(BiletMsg) - sizeof(long);

    loguj(C_KASA, "[KASA] Otwieram okienko (PID: %d)...\n", getpid());

    while(1) {
        // Odebranie zapytania o bilet
        odbierz_komunikat(msgid_req, &msg, rozmiar_danych, KANAL_KASA);

        if (msg.typ_pasazera == TYP_OPIEKUN) {
            loguj(C_KASA, "[KASA] Opsługuję opiekuna (PID: %d) i dziecko (TID: %lu)\n", 
            msg.pid_nadawcy, msg.tid_dziecka);
        } else {
            loguj(C_KASA, "[KASA] Obsługuję pasażera PID %d...\n", msg.pid_nadawcy);
        }
        
        usleep(50000); 

        // Odesłanie biletu
        if (msg.typ_pasazera != TYP_VIP) {
            msg.mtype = msg.pid_nadawcy; 
            wyslij_komunikat(msgid_res, &msg, rozmiar_danych);
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 3) return 1;
    int msgid_req = atoi(argv[1]);
    int msgid_res = atoi(argv[2]);
    kasjer_run(msgid_req, msgid_res);
    return 0;
}