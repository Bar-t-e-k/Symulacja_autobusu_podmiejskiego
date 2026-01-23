#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/msg.h>
#include <errno.h>
#include <signal.h>
#include "common.h"
#include "ipc_utils.h"
#include "logs.h"

// Kolor logów
#define C_KASA  "\033[1;31m"

// Flaga sterująca pętlą główną, zmieniana asynchronicznie przez handler sygnału
volatile sig_atomic_t uruchomiona = 1;

void handler_sigterm(int sig) {
    (void)sig;
    uruchomiona = 0;
}

// Logika kasjera
// Symuluje pracę okienka kasowego.
// Pasażerowie wysyłają żądania, a Kasa odsyła bilety.
void kasjer_run(int msgid_req, int msgid_res) {
    ustaw_sygnal(SIGTERM, handler_sigterm, 0);

    BiletMsg msg;
    int rozmiar_danych = sizeof(BiletMsg) - sizeof(long);

    loguj(C_KASA, "[KASA] Otwieram okienko (PID: %d)...\n", getpid());

    while(uruchomiona) {
        // Odbieranie żądań z priorytetem
        // Używana ujemna wartość typu wiadomości (-KANAL_KASA).
        // Funkcja najpierw sprawdzi, czy są wiadomości typu 1 (VIP).
        // Dopiero gdy ich nie ma, pobierze wiadomość typu 2 (Reszta).
        if (odbierz_komunikat(msgid_req, &msg, rozmiar_danych, -KANAL_KASA) == -1) {
            if (!uruchomiona) break;
            continue;
        }

        // VIP tylko "rejestruje" swoją obecność w systemie (logi).
        // Nie czeka na odpowiedź
        if (msg.mtype == KANAL_KASA_VIP) {
             loguj(C_KASA, "[KASA] VIP (PID: %d) - Rejestracja w systemie.\n", msg.pid_nadawcy);
             continue;
        }

        // Logowanie sprzedaży biletu. Rozróżnienie Opiekuna, aby odnotować w logach obecność dziecka.
        if (msg.typ_pasazera == TYP_OPIEKUN) {
            loguj(C_KASA, "[KASA] Obsługuję opiekuna (PID: %d) i dziecko (TID: %lu)\n", 
            msg.pid_nadawcy, msg.tid_dziecka);
        } else {
            loguj(C_KASA, "[KASA] Obsługuję pasażera PID %d...\n", msg.pid_nadawcy);
        }

        // Odesłanie biletu do nadawcy
        msg.mtype = msg.pid_nadawcy; 
        if (wyslij_komunikat(msgid_res, &msg, rozmiar_danych) == -1) {
             if (!uruchomiona) break;
        }
    }
    exit(0);
}

int main(int argc, char* argv[]) {
    if (argc < 3) return 1;
    int msgid_req = atoi(argv[1]);
    int msgid_res = atoi(argv[2]);
    kasjer_run(msgid_req, msgid_res);
    return 0;
}