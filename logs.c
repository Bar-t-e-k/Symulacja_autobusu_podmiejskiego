#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h> 
#include <sys/stat.h>
#include "logs.h"

#define LOG_FILE "symulacja.log"
#define LOG_BUFFER_SIZE 1024

// FUNKCJA LOGUJĄCA
// Wykorzystuje niskopoziomowe wywołania systemowe (write), aby zapewnić 
// maksymalną spójność logów przy setkach równolegle działających procesów.
void loguj(const char* kolor, const char* format, ...) {
    char bufor_msg[LOG_BUFFER_SIZE / 2]; // Bufor na samą treść komunikatu
    char bufor_out[LOG_BUFFER_SIZE];     // Bufor końcowy do zapisu
    int len;

    // Przetwarzanie zmiennej liczby argumentów
    va_list args;
    va_start(args, format);
    vsnprintf(bufor_msg, sizeof(bufor_msg), format, args);
    va_end(args);

    // Wyjście na ekran
    // Składanie komunikatu z kolorami. write() na STDOUT_FILENO jest 
    // bezpieczniejszy w środowisku wieloprocesowym niż printf.
    len = snprintf(bufor_out, sizeof(bufor_out), "%s%s%s", 
                   (kolor ? kolor : ""), 
                   bufor_msg, 
                   (kolor ? "\033[0m" : ""));

    if (len > 0) {
        write(STDOUT_FILENO, bufor_out, len);
    }

    // Zapis do pliku

    // Pobieranie aktualnego czasu systemowego dla każdego wpisu
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    char time_str[32];
    strftime(time_str, sizeof(time_str), "[%H:%M:%S] ", t);

    len = snprintf(bufor_out, sizeof(bufor_out), "%s%s", time_str, bufor_msg);

    // Otwarcie z flagą O_APPEND gwarantuje, że każde dopisanie do pliku 
    // nastąpi na jego końcu w sposób niepodzielny. Zapobiega to 
    // przeplataniu się komunikatów od różnych procesów.
    int fd = open(LOG_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd != -1) {
        if (len > 0) {
            write(fd, bufor_out, len);
        }
        close(fd);
    }
}

// Funkcja logująca błędy z errno
void loguj_blad(const char* prefix) {
    char* komunikat_bledu = strerror(errno);
    
    loguj("\033[1;31m", "[BŁĄD] %s: %s\n", prefix, komunikat_bledu);
}