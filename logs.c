#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include "logs.h"

#define LOG_FILE "symulacja.log"

void loguj(const char* kolor, const char* format, ...) {
    va_list args;
    
    // WYPISANIE NA EKRAN (Z KOLOREM)
    if (kolor) printf("%s", kolor);
    
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    if (kolor) printf("\033[0m"); // Reset koloru

    // ZAPIS DO PLIKU (BEZ KOLORU, Z DATĄ)
    FILE* f = fopen(LOG_FILE, "a");
    if (f) {
        // Pobranie czasu
        time_t now = time(NULL);
        struct tm* t = localtime(&now);
        
        // Zapis znacznika czasu [HH:MM:SS]
        fprintf(f, "[%02d:%02d:%02d] ", t->tm_hour, t->tm_min, t->tm_sec);

        // Zapis treści
        va_start(args, format);
        vfprintf(f, format, args);
        va_end(args);
        
        fclose(f);
    }
}

void loguj_blad(const char* prefix) {
    char* komunikat_bledu = strerror(errno);
    
    loguj("\033[1;31m", "[BŁĄD] %s: %s\n", prefix, komunikat_bledu);
}