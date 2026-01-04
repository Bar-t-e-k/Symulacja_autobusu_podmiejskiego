#ifndef ACTORS_H
#define ACTORS_H

void kasjer_run(int msgid);
void pasazer_run(int id, int shmid, int semid, int msgid, int typ);
void autobus_run(int id, int shmid, int semid);

#endif