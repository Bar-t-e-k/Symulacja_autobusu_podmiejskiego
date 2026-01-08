#ifndef CONFIG_H
#define CONFIG_H
#include "common.h"

void wczytaj_konfiguracje(const char* sciezka, SharedData* data);
void waliduj_konfiguracje(SharedData* data);

#endif