#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/msg.h>
#include <signal.h>
#include "common.h"
#include "ipc_utils.h"

// Kolory
#define C_BUS   "\033[1;33m" // Żółty
#define C_PAS   "\033[1;32m" // Zielony
#define C_VIP   "\033[1;35m" // Magenta
#define C_ROW   "\033[1;36m" // Cyjan
#define C_KASA  "\033[1;31m" // Czerwony
#define C_RST   "\033[0m"

volatile sig_atomic_t wymuszony_odjazd = 0;

// Obsługa sygnału 1 - wymuszony odjazd
void sygnal_odjazd(int sig) {
    (void)sig;
    wymuszony_odjazd = 1;
}

void kasjer_run(int msgid) {
    BiletMsg msg;
    int rozmiar_danych = sizeof(BiletMsg) - sizeof(long);

    printf(C_KASA "[KASA] Otwieram okienko (PID: %d)...\n" C_RST, getpid());

    while(1) {
        // Odebranie zapytania o bilet
        odbierz_komunikat(msgid, &msg, rozmiar_danych, KANAL_ZAPYTAN);

        // wyświetlenie informacji o obsługiwanym pasażerze
        // printf(C_KASA "[KASA] Obsługuję pasażera PID %d...\n" C_RST, msg.pid_nadawcy);
        
        // odesłanie biletu
        msg.mtype = msg.pid_nadawcy; 
        
        wyslij_komunikat(msgid, &msg, rozmiar_danych);
    }
}

void pasazer_run(int id, int shmid, int semid, int msgid, int typ) {
    SharedData* data = dolacz_pamiec(shmid);
    srand(time(NULL) ^ (getpid()<<16)); // Unikalne ziarno losowości

    char* kolor = C_PAS;
    char* nazwa = "Zwykły";
    int moje_drzwi = SEM_DRZWI_PAS;

    if (typ == TYP_VIP) { 
        kolor = C_VIP; 
        nazwa = "VIP"; 
        moje_drzwi = SEM_DRZWI_PAS;
    }
    
    if (typ == TYP_ROWER) { 
        kolor = C_ROW; 
        nazwa = "Rower"; 
        moje_drzwi = SEM_DRZWI_ROW;
    }

    zablokuj_semafor(semid, SEM_MUTEX);
    if (data->dworzec_otwarty == 0) {
        printf("%s[Pasażer %d] Dworzec zamknięty! Nie wchodzę.\n" C_RST, kolor, id);
        odblokuj_semafor(semid, SEM_MUTEX);
        odlacz_pamiec(data);
        exit(0);
    }
    odblokuj_semafor(semid, SEM_MUTEX);

    if (typ != TYP_VIP) {
        printf("%s[Pasażer %d (%s)] Idę do kasy (PID: %d).\n" C_RST, kolor, id, nazwa, getpid());
        
        BiletMsg bilet;
        bilet.mtype = KANAL_ZAPYTAN;
        bilet.pid_nadawcy = getpid();
        bilet.typ_pasazera = typ;
        
        int rozmiar = sizeof(BiletMsg) - sizeof(long);

        wyslij_komunikat(msgid, &bilet, rozmiar);

        odbierz_komunikat(msgid, &bilet, rozmiar, getpid());
        
        // printf("%s[Pasażer %d] Kupiłem bilet! Idę na peron.\n" C_RST, kolor, id);
    } else {
        printf("%s[Pasażer %d (VIP)] Mam karnet, omijam kolejkę do kasy.\n" C_RST, kolor, id);
    }
    
    printf("%s[Pasażer %d (%s)] Przychodzę na przystanek.\n" C_RST, kolor, id, nazwa);

    // VIP blokuje zwykłych
    if (typ == TYP_VIP) {
        zablokuj_semafor(semid, SEM_MUTEX);
        data->liczba_vip_oczekujacych++;
        odblokuj_semafor(semid, SEM_MUTEX);
    }

    int wszedlem = 0;
    while (!wszedlem) {
        // Sprawdzenie stanu dworca
        zablokuj_semafor(semid, moje_drzwi); // zajęcie drzwi przez odpowiedniego pasażera

        zablokuj_semafor(semid, SEM_MUTEX);

        if (data->dworzec_otwarty == 0) {
            printf("%s[Pasażer %d] Dworzec zamknięty! Wychodzę.\n" C_RST, kolor, id);
            if (typ == TYP_VIP) data->liczba_vip_oczekujacych--;
            
            odblokuj_semafor(semid, SEM_MUTEX);
            odblokuj_semafor(semid, moje_drzwi);
            break;
        }

        if (data->autobus_obecny == 1) {
            int mozna_wejsc = 0;

            if (typ == TYP_VIP) {
                if (data->liczba_pasazerow < P) mozna_wejsc = 1;
            }
            else if (typ == TYP_ROWER) {
                if (data->liczba_pasazerow < P && 
                    data->liczba_rowerow < R && 
                    data->liczba_vip_oczekujacych == 0) {
                    mozna_wejsc = 1;
                }
            }
            else { // Zwykły
                if (data->liczba_pasazerow < P && data->liczba_vip_oczekujacych == 0) {
                    mozna_wejsc = 1;
                }
            }
            
            if (mozna_wejsc) {
                data->liczba_pasazerow++;
                if (typ == TYP_ROWER) data->liczba_rowerow++;
                if (typ == TYP_VIP) data->liczba_vip_oczekujacych--;

                data->pasazerowie_obsluzeni++;
                data->calkowita_liczba_pasazerow++;

                printf("%s[Pasażer %d] WSIADŁEM (Drzwi: %s). Stan: P%d/R%d\n" C_RST, 
                       kolor, id, (typ==TYP_ROWER ? "Rower" : "Pasażer"), 
                       data->liczba_pasazerow, data->liczba_rowerow);
                
                wszedlem = 1;
            }
        }

        odblokuj_semafor(semid, SEM_MUTEX);

        if (wszedlem) usleep(50000);

        odblokuj_semafor(semid, moje_drzwi);

        if (!wszedlem) {
            usleep(100000); // Czeka 0.1s przed ponowną próbą
        }
    }

    odlacz_pamiec(data);
    exit(0);
}

void autobus_run(int id, int shmid, int semid) {
    signal(SIGUSR1, sygnal_odjazd);

    SharedData* data = dolacz_pamiec(shmid);
    int nr_kursu = 1;

    // Pętla pracy autobusu
    while (1) {
        zablokuj_semafor(semid, SEM_MUTEX);
        if (data->pasazerowie_obsluzeni >= data->limit_pasazerow || data->dworzec_otwarty == 0) {
            odblokuj_semafor(semid, SEM_MUTEX);
            break;
        }
        odblokuj_semafor(semid, SEM_MUTEX);

        // 1. Sprawdzenie czy można podjechać
        int czy_zamknac = 0;
        while (1) {
            zablokuj_semafor(semid, SEM_MUTEX);

            if (data->dworzec_otwarty == 0) {
                odblokuj_semafor(semid, SEM_MUTEX);
                czy_zamknac = 1; 
                break;
            }

            if (data->autobus_obecny == 0) {
                data->autobus_obecny = 1;
                data->pid_obecnego_autobusu = getpid();
                data->liczba_pasazerow = 0;
                data->liczba_rowerow = 0;
                odblokuj_semafor(semid, SEM_MUTEX);
                break;
            }
            odblokuj_semafor(semid, SEM_MUTEX);
            usleep(500000); // Czekaj 0.5s i sprawdź znowu
        }

        if (czy_zamknac) {
            break;
        }

        // 2. Otwieranie drzwi i czekanie
        printf(C_BUS "[Autobus %d] Podjeżdża na peron (Kurs %d).\n" C_RST, id, nr_kursu);
        printf(C_BUS "[Autobus %d] Otwiera drzwi na %d s...\n" C_RST, id, T_ODJAZD);
        
        int czas_pozostaly = sleep(T_ODJAZD);
        if (wymuszony_odjazd) {
            if (data->dworzec_otwarty == 0) {
                printf(C_BUS "\n[Autobus %d] Dworzec zamknięty! Odjazd z pasażerami w środku (Zaoszczędzono %d sekund)\n" C_RST, id, czas_pozostaly);
            } else {
                printf(C_BUS "\n[Autobus %d] Wymuszony odjazd przez dyspozytora! (Sygnał 1)\n" C_RST, id);
            }
            wymuszony_odjazd = 0; // reset flagi
        }

        // 3. Odjazd
        zablokuj_semafor(semid, SEM_MUTEX);
        data->autobus_obecny = 0; // Zamyka drzwi
        data->pid_obecnego_autobusu = 0;
        int zabranych = data->liczba_pasazerow;
        int rowery = data->liczba_rowerow;
        odblokuj_semafor(semid, SEM_MUTEX);

        printf(C_BUS "[Autobus %d] ODJAZD z %d pasażerami oraz %d rowerami! Trasa...\n" C_RST, id, zabranych, rowery);
        
        // Symulacja jazdy
        sleep(CZAS_PRZEJAZDU); 
        printf(C_BUS "[Autobus %d] Wrócił z trasy.\n" C_RST, id);

        nr_kursu++;
    }

    zablokuj_semafor(semid, SEM_MUTEX);
    data->aktywne_autobusy--;
    printf(C_BUS "[Autobus %d] Koniec zmiany. Pozostało autobusów: %d\n" C_RST, id, data->aktywne_autobusy);
    odblokuj_semafor(semid, SEM_MUTEX);

    odlacz_pamiec(data);
    exit(0);
}