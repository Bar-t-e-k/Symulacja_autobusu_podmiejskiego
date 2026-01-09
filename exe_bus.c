#include <stdlib.h>
#include "actors.h"

int g_shmid = -1, g_semid = -1, g_msgid = -1;

int main(int argc, char* argv[]) {
    if (argc < 5) return 1;

    int id = atoi(argv[1]);
    g_shmid = atoi(argv[2]);
    g_semid = atoi(argv[3]);
    g_msgid = atoi(argv[4]);

    autobus_run(id, g_shmid, g_semid, g_msgid);
    return 0;
}