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

// Kolory
#define C_BUS   "\033[1;33m" // Żółty
#define C_PAS   "\033[1;32m" // Zielony
#define C_OPIE "\033[1;34m" // Niebieski
#define C_DZIE "\033[1;37m" // Biały
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
        printf("%s[Pasażer %d] Dworzec zamknięty! Nie wchodzę.\n" C_RST, kolor, id);
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
        printf("%s[Dziecko %d] Czekam na rodzica...\n" C_RST, kolor, id);

        BiletMsg bilet;
        odbierz_komunikat(msgid, &bilet, sizeof(BiletMsg) - sizeof(long), getpid());
    } else if (typ != TYP_VIP) {
        printf("%s[Pasażer %d (%s)] Idę do kasy (PID: %d).\n" C_RST, kolor, id, nazwa, getpid());

        int ile_biletow = (typ == TYP_OPIEKUN) ? 2 : 1;
        
        for (int i = 0; i < ile_biletow; i++) {
            BiletMsg bilet;
            bilet.mtype = KANAL_ZAPYTAN;
            bilet.pid_nadawcy = getpid();
            bilet.typ_pasazera = typ;
        
            int rozmiar = sizeof(BiletMsg) - sizeof(long);

            wyslij_komunikat(msgid, &bilet, rozmiar);

            odbierz_komunikat(msgid, &bilet, rozmiar, getpid());
        
            // printf("%s[Pasażer %d] Kupiłem bilet! Idę na peron.\n" C_RST, kolor, id);
            } 
    } else {
        printf("%s[Pasażer %d (VIP)] Mam karnet, omijam kolejkę do kasy.\n" C_RST, kolor, id);
    }

    if (typ == TYP_OPIEKUN) {
        BiletMsg dla_dziecka;
        dla_dziecka.mtype = pid_dziecka;
        dla_dziecka.pid_nadawcy = getpid();
        wyslij_komunikat(msgid, &dla_dziecka, sizeof(BiletMsg) - sizeof(long));
    }
    
    printf("%s[Pasażer %d (%s)] Przychodzę na przystanek.\n" C_RST, kolor, id, nazwa);

    int wszedlem = 0;
    while (!wszedlem) {
        if (typ != TYP_VIP && typ != TYP_DZIECKO) zablokuj_semafor(semid, moje_drzwi);
        
        zablokuj_semafor(semid, SEM_MUTEX);
        data = dolacz_pamiec(shmid); // Odświeżenie wskaźnika po możliwej zmianie

        if (data->dworzec_otwarty == 0) {
            printf("%s[Pasażer %d] Dworzec zamknięty! Wychodzę.\n" C_RST, kolor, id);
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
            
            usleep(200000); 
            continue;
        }

        if (data->autobus_obecny == 1) {
            int zgoda = 0;
            int czy_probowac = 1;

            if (typ == TYP_OPIEKUN) {
                if (data->liczba_pasazerow + 2 > P) {
                    //printf("%s[Pasażer %d (Opiekun)] Za mało miejsca dla mnie i dziecka. Czekam na następny autobus.\n" C_RST, kolor, id);
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

                    if (odbierz_z_timeoutem(msgid, &odpowiedz, sizeof(BiletMsg) - sizeof(long), getpid(), 50)) {
                        if (odpowiedz.pid_nadawcy != -1) {
                            zgoda = 1; // Kierowca zgadza się na wejście
                        }
                    }
                }
            }
            if (zgoda) {
                zablokuj_semafor(semid, SEM_MUTEX);
                data = dolacz_pamiec(shmid); // Odświeżenie wskaźnika po możliwej zmianie

                printf("%s[Pasażer %d (%s)] Wszedłem do autobusu! (Obecnie w środku: %d osób i %d rowerów)\n" C_RST, kolor, id, nazwa, data->liczba_pasazerow, data->liczba_rowerow);

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
                        printf(C_KASA "[Opiekun %d] Błąd: Kierowca nie potwierdził wejścia dziecka!\n" C_RST, id);
                    }
                }
            } else {
                //printf("%s[Pasażer %d (%s)] Kierowca nie pozwolił mi wejść. Czekam na następny autobus.\n" C_RST, kolor, id, nazwa);
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

void autobus_run(int id, int shmid, int semid) {
    signal(SIGUSR1, sygnal_odjazd);
    
    int msgid = stworz_kolejke(); 
    SharedData* data;
    
    srand(time(NULL) ^ (getpid() << 16));

    printf(C_BUS "[Autobus %d] Zgłasza gotowość na dworcu. Czekam na wjazd...\n" C_RST, id);

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

        printf(C_BUS "[Autobus %d] Podstawiłem się. SPRAWDZAM BILETY!\n" C_RST, id);

        // ZAŁADUNEK (Aktywny Kierowca)
        time_t start = time(NULL);
        
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
                odp.pid_nadawcy = getpid();

                if (mozna) {
                    data->liczba_pasazerow++;
                    if (bilet.typ_pasazera == TYP_ROWER) data->liczba_rowerow++;
                    
                    data->pasazerowie_obsluzeni++;
                    data->calkowita_liczba_pasazerow++;

                    odp.typ_pasazera = bilet.typ_pasazera;
                } else {
                    odp.pid_nadawcy = -1; // Odrzucenie wejścia
                }
                
                wyslij_komunikat(msgid, &odp, sizeof(BiletMsg) - sizeof(long));
                odlacz_pamiec(data);
                odblokuj_semafor(semid, SEM_MUTEX);

                continue;
            }
            usleep(10000); 
        }

        if (wymuszony_odjazd) {
            printf(C_BUS "[Autobus %d] DYSPOZYTOR KAŻE JECHAĆ!\n" C_RST, id);
            wymuszony_odjazd = 0;
        }

        // ODJAZD
        zablokuj_semafor(semid, SEM_MUTEX);
        data = dolacz_pamiec(shmid);
        
        data->autobus_obecny = 0; 
        int p = data->liczba_pasazerow;
        int r = data->liczba_rowerow;
        
        odlacz_pamiec(data);
        odblokuj_semafor(semid, SEM_MUTEX);

        printf(C_BUS "[Autobus %d] ODJAZD z %d pasażerami (%d rowerów).\n" C_RST, id, p, r);

        // JAZDA (Losowy czas)
        int czas_trasy = 5 + (rand() % 11); 
        sleep(czas_trasy);

        printf(C_BUS "[Autobus %d] WRÓCIŁ Z TRASY po %d s.\n" C_RST, id, czas_trasy);
        
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
    printf(C_BUS "[Autobus %d] Zjazd do zajezdni (Koniec pracy).\n" C_RST, id);
    odlacz_pamiec(data);
    odblokuj_semafor(semid, SEM_MUTEX);
    
    exit(0);
}