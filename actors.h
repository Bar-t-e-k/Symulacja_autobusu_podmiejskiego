#ifndef ACTORS_H
#define ACTORS_H

void sygnal_odjazd(int sig);
void kasjer_run(int msgid);
void pasazer_run(int id, int shmid, int semid, int msgid, int typ, pid_t pid_dziecka);
void autobus_run(int id, int shmid, int semid, int msgid);

#endif