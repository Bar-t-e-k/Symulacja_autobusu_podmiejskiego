#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/msg.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include "common.h"
#include "ipc_utils.h"
#include "logs.h"

// Kolory
#define C_BUS   "\033[1;33m" // Żółty
#define C_PAS   "\033[1;32m" // Zielony
#define C_OPIE "\033[1;34m" // Niebieski
#define C_DZIE "\033[1;37m" // Biały
#define C_VIP   "\033[1;35m" // Magenta
#define C_ROW   "\033[1;36m" // Cyjan
#define C_KASA  "\033[1;31m" // Czerwony

volatile sig_atomic_t wymuszony_odjazd = 0;

// Obsługa sygnału 1 - wymuszony odjazd
void sygnal_odjazd(int sig) {
    (void)sig;
    wymuszony_odjazd = 1;
}

// Funkcja pomocnicza: odbiera z timeoutem (zwraca 1 gdy OK, 0 gdy timeout)
int odbierz_z_timeoutem(int msgid, void* msg, int rozmiar, long typ, int max_prob) {
    for (int i = 0; i < max_prob; i++) {
        if (odbierz_komunikat_bez_blokowania(msgid, msg, rozmiar, typ)) {
            return 1; 
        }
        if (errno != ENOMSG) {
            return 0;
        }
        usleep(100000); // Czekaj 0.1s
    }
    return 0; // Timeout
}

void kasjer_run(int msgid) {
    BiletMsg msg;
    int rozmiar_danych = sizeof(BiletMsg) - sizeof(long);

    loguj(C_KASA, "[KASA] Otwieram okienko (PID: %d)...\n", getpid());

    while(1) {
        // Odebranie zapytania o bilet
        odbierz_komunikat(msgid, &msg, rozmiar_danych, KANAL_ZAPYTAN);

        // wyświetlenie informacji o obsługiwanym pasażerze
        loguj(C_KASA, "[KASA] Obsługuję pasażera PID %d...\n", msg.pid_nadawcy);
        
        // odesłanie biletu
        msg.mtype = msg.pid_nadawcy; 
        
        wyslij_komunikat(msgid, &msg, rozmiar_danych);
    }
}

void pasazer_run(int id, int shmid, int semid, int msgid, int typ, pid_t pid_dziecka) {
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

    if (typ == TYP_OPIEKUN) { 
        kolor = C_OPIE; 
        nazwa = "Opiekun"; 
        moje_drzwi = SEM_DRZWI_PAS;
    }

    if (typ == TYP_DZIECKO) { 
        kolor = C_DZIE; 
        nazwa = "Dziecko"; 
        moje_drzwi = SEM_DRZWI_PAS;
    }

    zablokuj_semafor(semid, SEM_MUTEX);
    if (data->dworzec_otwarty == 0) {
        loguj(kolor, "[Pasażer %d] Dworzec zamknięty! Nie wchodzę.\n", id);
        odblokuj_semafor(semid, SEM_MUTEX);
        odlacz_pamiec(data);
        exit(0);
    }
    data->liczba_oczekujacych++;

    if (typ == TYP_VIP) {
        data->liczba_vip_oczekujacych++;
    }

    odblokuj_semafor(semid, SEM_MUTEX);

    if (typ == TYP_DZIECKO) {
        loguj(kolor, "[Dziecko %d] Czekam na rodzica...\n", id);

        BiletMsg bilet;
        odbierz_komunikat(msgid, &bilet, sizeof(BiletMsg) - sizeof(long), getpid());
    } else if (typ != TYP_VIP) {
        loguj(kolor, "[Pasażer %d (%s)] Idę do kasy (PID: %d).\n", id, nazwa, getpid());

        int ile_biletow = (typ == TYP_OPIEKUN) ? 2 : 1;
        
        for (int i = 0; i < ile_biletow; i++) {
            BiletMsg bilet;
            bilet.mtype = KANAL_ZAPYTAN;
            bilet.pid_nadawcy = getpid();
            bilet.typ_pasazera = typ;
        
            int rozmiar = sizeof(BiletMsg) - sizeof(long);

            wyslij_komunikat(msgid, &bilet, rozmiar);

            odbierz_komunikat(msgid, &bilet, rozmiar, getpid());
            } 
    } else {
        loguj(kolor, "[Pasażer %d (VIP)] Mam karnet, omijam kolejkę do kasy.\n", id);
    }

    if (typ == TYP_OPIEKUN) {
        BiletMsg dla_dziecka;
        dla_dziecka.mtype = pid_dziecka;
        dla_dziecka.pid_nadawcy = getpid();
        wyslij_komunikat(msgid, &dla_dziecka, sizeof(BiletMsg) - sizeof(long));
    }

    loguj(kolor, "[Pasażer %d (%s)] Przychodzę na przystanek.\n", id, nazwa);

    int wszedlem = 0;
    while (!wszedlem) {
        if (typ != TYP_VIP && typ != TYP_DZIECKO) zablokuj_semafor(semid, moje_drzwi);
        
        zablokuj_semafor(semid, SEM_MUTEX);
        data = dolacz_pamiec(shmid); // Odświeżenie wskaźnika po możliwej zmianie

        if (data->dworzec_otwarty == 0) {
            loguj(kolor, "[Pasażer %d] Dworzec zamknięty! Wychodzę.\n", id);
            data->liczba_oczekujacych--;
            if (typ == TYP_VIP) data->liczba_vip_oczekujacych--;

            odlacz_pamiec(data);
            odblokuj_semafor(semid, SEM_MUTEX);

            if (typ != TYP_VIP && typ != TYP_DZIECKO) odblokuj_semafor(semid, moje_drzwi);
            
            exit(0);
        }

        if (typ != TYP_VIP && data->liczba_vip_oczekujacych > 0) {
            odlacz_pamiec(data); 
            odblokuj_semafor(semid, SEM_MUTEX);
            
            if (typ != TYP_VIP && typ != TYP_DZIECKO) odblokuj_semafor(semid, moje_drzwi);
            
            usleep(100000); 
            continue;
        }

        int stan_ludzi = 0;
        int stan_rowerow = 0;

        if (data->autobus_obecny == 1) {
            int zgoda = 0;
            int czy_probowac = 1;

            if (typ == TYP_OPIEKUN) {
                if (data->liczba_pasazerow + 2 > P) {
                    czy_probowac = 0;
                }
            }

            odlacz_pamiec(data); odblokuj_semafor(semid, SEM_MUTEX); // Zwalnia mutex na czas rozmowy z kierowcą

            if (czy_probowac) {
                if (typ == TYP_DZIECKO) {
                    BiletMsg wolanie;
                    odbierz_komunikat(msgid, &wolanie, sizeof(BiletMsg) - sizeof(long), getpid());
                    zgoda = 1; // Dziecko zawsze wchodzi z opiekunem
                } else {
                    // Rozmowa z kierowcą
                    BiletMsg bilet;
                    bilet.mtype = KANAL_KONTROLA;
                    bilet.pid_nadawcy = getpid();
                    bilet.typ_pasazera = typ;

                    wyslij_komunikat(msgid, &bilet, sizeof(BiletMsg) - sizeof(long));

                    BiletMsg odpowiedz;

                    if (odbierz_z_timeoutem(msgid, &odpowiedz, sizeof(BiletMsg) - sizeof(long), getpid(), 20)) {
                        if (odpowiedz.pid_nadawcy != -1) {
                            zgoda = 1; // Kierowca zgadza się na wejście
                            stan_ludzi = odpowiedz.typ_pasazera;
                            stan_rowerow = odpowiedz.pid_nadawcy;
                        }
                    }
                }
            }
            if (zgoda) {
                zablokuj_semafor(semid, SEM_MUTEX);

                if (typ == TYP_DZIECKO) {
                    // Dziecko musi doczytać z pamięci (bo nie dostało biletu od kierowcy)
                    data = dolacz_pamiec(shmid);
                    stan_ludzi = data->liczba_pasazerow;
                    stan_rowerow = data->liczba_rowerow;
                }

                loguj(kolor, "[Pasażer %d (%s)] Wszedłem do autobusu!\n", id, nazwa);
                loguj(C_BUS, "   -> [Stan Autobusu] Pasażerów: %d/%d (Rowerów: %d/%d)\n", stan_ludzi, P, stan_rowerow, R);

                if (typ != TYP_DZIECKO) data = dolacz_pamiec(shmid);

                data->liczba_oczekujacych--;
                if (typ == TYP_VIP) data->liczba_vip_oczekujacych--;
                
                odlacz_pamiec(data);
                odblokuj_semafor(semid, SEM_MUTEX);
                wszedlem = 1;

                if (typ == TYP_DZIECKO) {
                     // Dziecko wysyła potwierdzenie do rodzica
                     BiletMsg potwierdzenie; 
                     potwierdzenie.mtype = getppid();
                     wyslij_komunikat(msgid, &potwierdzenie, sizeof(BiletMsg) - sizeof(long));
                }
                
                if (typ == TYP_OPIEKUN) {
                    BiletMsg bilet;
                    bilet.mtype = KANAL_KONTROLA;
                    bilet.pid_nadawcy = getpid();
                    bilet.typ_pasazera = TYP_DZIECKO; 
                    
                    wyslij_komunikat(msgid, &bilet, sizeof(BiletMsg) - sizeof(long));

                    BiletMsg odp;
                    // Czeka na potwierdzenie dla dziecka
                    if (odbierz_z_timeoutem(msgid, &odp, sizeof(BiletMsg) - sizeof(long), getpid(), 50)) {
                        // Kierowca wpuścił dziecko
                        BiletMsg wolanie; 
                        wolanie.mtype = pid_dziecka;
                        wyslij_komunikat(msgid, &wolanie, sizeof(BiletMsg) - sizeof(long));

                        BiletMsg potwierdzenie;
                        odbierz_z_timeoutem(msgid, &potwierdzenie, sizeof(BiletMsg) - sizeof(long), getpid(), 50);
                    } else {
                        loguj(C_KASA, "[Opiekun %d] Błąd: Kierowca nie potwierdził wejścia dziecka!\n", id);
                    }
                }
            }
        } else {
            odlacz_pamiec(data); odblokuj_semafor(semid, SEM_MUTEX);
        }
        if (typ != TYP_VIP && typ != TYP_DZIECKO) odblokuj_semafor(semid, moje_drzwi);

        if (wszedlem) {
            exit(0);
        } else {
            usleep(500000); // Czekaj 0.5s i sprawdź znowu
        }
    }
}

void autobus_run(int id, int shmid, int semid, int msgid) {
    signal(SIGUSR1, sygnal_odjazd);
    
    SharedData* data;
    
    srand(time(NULL) ^ (getpid() << 16));

    loguj(C_BUS, "[Autobus %d] Zgłasza gotowość na dworcu. Czekam na wjazd...\n", id);

    while (1) {
        int czy_koniec_pracy = 0;

        while(1) {
             zablokuj_semafor(semid, SEM_MUTEX);
             data = dolacz_pamiec(shmid);
             
             // Sprawdzenie czy koniec pracy
             if (data->dworzec_otwarty == 0) { 
                 odlacz_pamiec(data); 
                 odblokuj_semafor(semid, SEM_MUTEX); 
                 czy_koniec_pracy = 1; 
                 break; 
             }
             
             // Sprawdzenie czy stanowisko wolne
             if (data->autobus_obecny == 0) {
                 data->autobus_obecny = 1; 
                 data->pid_obecnego_autobusu = getpid();
                 data->liczba_pasazerow = 0; 
                 data->liczba_rowerow = 0;
                 
                 odlacz_pamiec(data);
                 odblokuj_semafor(semid, SEM_MUTEX); 
                 break;
             }
             
             odlacz_pamiec(data);
             odblokuj_semafor(semid, SEM_MUTEX); 
             usleep(200000); 
        }

        // Jeśli w pętli wewnętrznej wykryto koniec pracy, przerywamy też główną pętlę
        if (czy_koniec_pracy) {
            break; 
        }

        loguj(C_BUS, "[Autobus %d] Podstawiłem się. SPRAWDZAM BILETY!\n", id);

        // ZAŁADUNEK (Aktywny Kierowca)
        time_t start = time(NULL);

        int oczekuje_na_dziecko = 0;
        
        while(time(NULL) - start < T_ODJAZD && !wymuszony_odjazd) {
            BiletMsg bilet;
            int rozmiar = sizeof(BiletMsg) - sizeof(long);
            
            if (odbierz_komunikat_bez_blokowania(msgid, &bilet, rozmiar, KANAL_KONTROLA)) {
                
                zablokuj_semafor(semid, SEM_MUTEX);
                data = dolacz_pamiec(shmid);
                
                int mozna = 1;
                if (data->liczba_pasazerow >= P) mozna = 0;
                if (bilet.typ_pasazera == TYP_ROWER && data->liczba_rowerow >= R) mozna = 0;

                BiletMsg odp; 
                memset(&odp, 0, sizeof(BiletMsg));
                odp.mtype = bilet.pid_nadawcy;

                if (mozna) {
                    data->liczba_pasazerow++;
                    if (bilet.typ_pasazera == TYP_ROWER) data->liczba_rowerow++;
                    
                    data->pasazerowie_obsluzeni++;
                    data->calkowita_liczba_pasazerow++;

                    odp.typ_pasazera = data->liczba_pasazerow;
                    odp.pid_nadawcy = data->liczba_rowerow;

                    if (bilet.typ_pasazera == TYP_OPIEKUN) {
                        oczekuje_na_dziecko = 1;
                    } else {
                        oczekuje_na_dziecko = 0;
                    }
                } else {
                    odp.pid_nadawcy = -1; // Odrzucenie wejścia

                    if (bilet.typ_pasazera == TYP_OPIEKUN) {
                        oczekuje_na_dziecko = 0; 
                    }
                }
                
                wyslij_komunikat(msgid, &odp, sizeof(BiletMsg) - sizeof(long));
                odlacz_pamiec(data);
                odblokuj_semafor(semid, SEM_MUTEX);

                continue;
            }
            usleep(10000); 
        }

        if (wymuszony_odjazd) {
            loguj(C_BUS, "[Autobus %d] DYSPOZYTOR KAŻE JECHAĆ!\n", id);
            wymuszony_odjazd = 0;
        }

        // OSTATNIE SPRAWDZENIE (PO CZASIE)
        // Odrzucenie wszystkich, chyba że czekamy na dziecko opiekuna, który właśnie wszedł
        while (1) {
            BiletMsg bilet;
            int rozmiar = sizeof(BiletMsg) - sizeof(long);
            
            if (odbierz_komunikat_bez_blokowania(msgid, &bilet, rozmiar, KANAL_KONTROLA)) {
                
                BiletMsg odp; 
                memset(&odp, 0, sizeof(BiletMsg));
                odp.mtype = bilet.pid_nadawcy;
                odp.pid_nadawcy = getpid();
                
                // SPRAWDZENIE WYJĄTKU DLA DZIECKA
                int wyjatek_dla_dziecka = 0;
                if (oczekuje_na_dziecko && bilet.typ_pasazera == TYP_DZIECKO) {
                    wyjatek_dla_dziecka = 1;
                }

                if (wyjatek_dla_dziecka) {
                    zablokuj_semafor(semid, SEM_MUTEX);
                    data = dolacz_pamiec(shmid);
                    
                    data->liczba_pasazerow++;
                    data->pasazerowie_obsluzeni++; 
                    data->calkowita_liczba_pasazerow++;

                    odp.typ_pasazera = data->liczba_pasazerow;
                    odp.pid_nadawcy = data->liczba_rowerow;
                    
                    loguj(C_BUS, "[Autobus %d] Czas minął, ale doczekałem na Dziecko!\n", id);
                    
                    odlacz_pamiec(data); 
                    odblokuj_semafor(semid, SEM_MUTEX);
                    
                    odp.typ_pasazera = bilet.typ_pasazera; // Zgoda
                    oczekuje_na_dziecko = 0; 
                } 
                else {
                    odp.pid_nadawcy = -1; 
                }
                
                wyslij_komunikat(msgid, &odp, rozmiar);
                continue; 
            } else {
                break;
            }
        }

        // ODJAZD
        zablokuj_semafor(semid, SEM_MUTEX);
        data = dolacz_pamiec(shmid);
        
        data->autobus_obecny = 0; 
        int p = data->liczba_pasazerow;
        int r = data->liczba_rowerow;
        
        odlacz_pamiec(data);
        odblokuj_semafor(semid, SEM_MUTEX);

        loguj(C_BUS, "[Autobus %d] ODJAZD z %d pasażerami (%d rowerów).\n", id, p, r);

        // JAZDA (Losowy czas)
        int czas_trasy = 5 + (rand() % 11); 
        sleep(czas_trasy);

        loguj(C_BUS, "[Autobus %d] WRÓCIŁ Z TRASY po %d s.\n", id, czas_trasy);
        
        // Sprawdzenie czy kontynuować po powrocie 
        // W razie gdyby dworzec zamknięto w trakcie jazdy
        zablokuj_semafor(semid, SEM_MUTEX);
        data = dolacz_pamiec(shmid);
        if (data->dworzec_otwarty == 0) {
            odlacz_pamiec(data);
            odblokuj_semafor(semid, SEM_MUTEX);
            break; 
        }
        odlacz_pamiec(data);
        odblokuj_semafor(semid, SEM_MUTEX);
    }

    // SPRZĄTANIE
    zablokuj_semafor(semid, SEM_MUTEX);
    data = dolacz_pamiec(shmid);
    data->aktywne_autobusy--;
    loguj(C_BUS, "[Autobus %d] Zjazd do zajezdni (Koniec pracy).\n", id);
    odlacz_pamiec(data);
    odblokuj_semafor(semid, SEM_MUTEX);
    
    exit(0);
}