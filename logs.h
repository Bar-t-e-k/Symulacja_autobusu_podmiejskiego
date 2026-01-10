#ifndef LOGGER_H
#define LOGGER_H

// Funkcja logująca
void loguj(const char* kolor, const char* format, ...);

// Funkcja logująca błędy, działa jak perror, ale zapisuje też do pliku
void loguj_blad(const char* prefix);

#endif