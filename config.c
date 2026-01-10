#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "logs.h"

// Funkcja wczytująca konfigurację z pliku config.txt
void wczytaj_konfiguracje(const char* sciezka, SharedData* data) {
    FILE* f = fopen(sciezka, "r");
    if (!f) {
        loguj_blad("Nie można otworzyć pliku config.txt");
        exit(1);
    }

    // Domyślne wartości w razie braków w pliku
    data->cfg_P = 10;
    data->cfg_R = 0;
    data->cfg_N = 1;
    data->cfg_TP = 10;
    data->cfg_LiczbaPas = 20;

    char bufor[128];
    while (fgets(bufor, sizeof(bufor), f)) {
        char klucz[32];
        int wartosc;
        
        // KLUCZ=WARTOSC
        if (sscanf(bufor, "%31[^=]=%d", klucz, &wartosc) == 2) {
            if (strcmp(klucz, "P") == 0) data->cfg_P = wartosc;
            else if (strcmp(klucz, "R") == 0) data->cfg_R = wartosc;
            else if (strcmp(klucz, "N") == 0) data->cfg_N = wartosc;
            else if (strcmp(klucz, "T_POSTOJ") == 0) data->cfg_TP = wartosc;
            else if (strcmp(klucz, "L_PASAZEROW") == 0) data->cfg_LiczbaPas = wartosc;
        }
    }
    fclose(f);
}

// Funkcja walidująca konfigurację
void waliduj_konfiguracje(SharedData* data) {
    int bledy = 0;

    if (data->cfg_P <= 0) {
        loguj(NULL, "[BŁĄD KONFIGURACJI] Pojemność P musi być > 0 (jest %d)\n", data->cfg_P);
        bledy++;
    }

    if (data->cfg_R < 0) {
        loguj(NULL, "[BŁĄD KONFIGURACJI] Miejsca na rowery R muszą być >= 0\n");
        bledy++;
    }

    if (data->cfg_R > data->cfg_P) {
        loguj(NULL, "[BŁĄD KONFIGURACJI] R (%d) nie może być większe niż P (%d)\n", data->cfg_R, data->cfg_P);
        bledy++;
    }

    if (data->cfg_N <= 0) {
        loguj(NULL, "[BŁĄD KONFIGURACJI] Liczba autobusów N musi być > 0\n");
        bledy++;
    }
    
    if (data->cfg_LiczbaPas <= 0) {
        loguj(NULL, "[BŁĄD KONFIGURACJI] Liczba pasażerów musi być > 0\n");
        bledy++;
    }

    if (bledy > 0) {
        loguj(NULL, "[FATAL] Znaleziono %d błędów w konfiguracji. Popraw konfigurację i spróbuj ponownie.\n", bledy);
        exit(1);
    }
    
    loguj(NULL, "Konfiguracja: P=%d, R=%d, N=%d, Pasażerów=%d\n", 
          data->cfg_P, data->cfg_R, data->cfg_N, data->cfg_LiczbaPas);
}