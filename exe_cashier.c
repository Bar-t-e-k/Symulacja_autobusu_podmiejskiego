#include <stdlib.h>
#include "actors.h"

int g_shmid = -1, g_semid = -1, g_msgid = -1;

int main(int argc, char* argv[]) {
    if (argc < 2) return 1;
    
    g_msgid = atoi(argv[1]);
    kasjer_run(g_msgid);
    return 0;
}