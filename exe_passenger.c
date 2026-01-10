#include <stdlib.h>
#include <sys/types.h>
#include "actors.h"

// Zmienne globalne wymagane przez signals.o
int g_shmid = -1, g_semid = -1, g_msgid = -1;

int main(int argc, char* argv[]) {
    if (argc < 7) return 1;

    int id = atoi(argv[1]);
    g_shmid = atoi(argv[2]);
    g_semid = atoi(argv[3]);
    g_msgid = atoi(argv[4]);
    int typ = atoi(argv[5]);
    pid_t pid_dziecka = (pid_t)atoi(argv[6]);

    pasazer_run(id, g_shmid, g_semid, g_msgid, typ, pid_dziecka);
    return 0;
}