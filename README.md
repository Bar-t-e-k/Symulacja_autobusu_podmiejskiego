# 🚌 Symulator Dworca Autobusowego

Projekt realizuje zaawansowaną symulację dworca autobusowego, demonstrując praktyczne zastosowanie mechanizmów Inter-Process Communication (IPC).

Projekt kładzie nacisk na synchronizację zasobów, obsługę sygnałów asynchronicznych oraz odporność na błędy.

---

## 🚀 Funkcjonalności

### 1. Architektura Wieloprocesowa
Symulacja wykorzystuje funkcje `fork()` oraz rodzinę funkcji `execlp()` do tworzenia niezależnych procesów dla każdego aktora systemu:
* **Główny Zarządca (`main`)**: Inicjuje zasoby, tworzy procesy potomne, monitoruje stan symulacji, sprząta na bieżąco i po zakończeniu oraz **pełni rolę Dyspozytora** (obsługuje sterowanie z klawiatury).
* **Autobusy**: Niezależne procesy realizujące kursy, zabierające pasażerów i obsługujące limity miejsc.
* **Pasażerowie**: Procesy symulujące zachowanie ludzi (kupno biletu, oczekiwanie, wejście).
* **Kasjer**: Proces obsługujący kolejki komunikatów z zapytaniami o bilety.

### 2. Typy Pasażerów i Logika Biznesowa
System obsługuje różne typy pasażerów ze specyficznymi zachowaniami:
* **🟢 Pasażer Zwykły**: Musi kupić bilet w kasie, a następnie czeka w kolejce do autobusu.
* **🟣 VIP**: Posiada priorytet – omija kolejkę do kasy oraz ma pierwszeństwo wejścia do autobusu przed zwykłymi pasażerami.
* **🔵 Rodzina (Opiekun + Dziecko)**: Zaimplementowana jako jeden proces z wątkiem. Proces Opiekuna sprawdza dostępność dwóch miejsc. Dziecko jest realizowane jako wątek `pthread`, który towarzyszy procesowi rodzica.
* **🚲 Rowerzysta**: Zajmuje miejsce pasażerskie ORAZ limitowane miejsce na rower. Używa osobnego wejścia.

### 3. Mechanizmy IPC
* **Pamięć Dzielona (Shared Memory)**: Przechowuje globalny stan dworca (m.in. liczniki pasażerów, flagi otwarcia, PID obecnego autobusu).
* **Semafory (Semaphores)**:
    * `SEM_MUTEX` - z flagą `SEM_UNDO`: Gwarantuje wyłączny dostęp do pamięci dzielonej (sekcja krytyczna).
    * `SEM_DRZWI_PAS`: Ogranicza przepustowość wejścia pasażerskiego.
    * `SEM_DRZWI_ROW`: Ogranicza przepustowość wejścia dla rowerów.
    * `SEM_KOLEJKA_VIP`: Realizuje priorytetowe wejście dla pasażerów VIP, wybudzając ich przed pozostałymi grupami.
    * `SEM_PRZYSTANEK` - z flagą `SEM_UNDO`: Gwarantuje, że na peronie znajduje się tylko jeden autobus (synchronizacja wjazdu na stanowisko).
    * `SEM_KTOS_CZEKA`: Działa jako "dzwonek" – pasażerowie informują nim autobus o swojej obecności na przystanku.
    * `SEM_LIMIT`: Ogranicza maksymalną liczbę procesów przebywających jednocześnie na dworcu (zapobiega przepełnieniu systemu).
    * `SEM_WSIADL`: Sygnał zwrotny od pasażera do autobusu, potwierdzający zakończenie procedury wsiadania.
* **Kolejki Komunikatów (Message Queues)**:
    * Komunikacja `Pasażer <-> Kasjer` (symulacja zakupu biletu) (2 kolejki do obsługi w dwie strony); dodatkowo priorytetowa obsługa pasażerów VIP.

### 4. Sygnały (Signals)
* `SIGALRM`: Przerywa blokujące operacje w autobusie (Timeout).
* `SIGUSR1`: Wymuszony odjazd autobusu (Obsługa w `exe_bus`).
* `SIGUSR2`: Ewakuacja dworca (Obsługa we wszystkich procesach).
* `SIGINT`: Bezpieczne zakończenie (przechwytywanie Ctrl+C).

### 5. Bezpieczeństwo i Logowanie
* **Graceful Shutdown**: System gwarantuje usunięcie zasobów IPC (pamięć, semafory) niezależnie od sposobu zakończenia programu (sygnał `SIGINT`, błąd, czy normalne zakończenie) dzięki `atexit()`.
* **Deadlock Prevention**: Autobus nie blokuje semaforów drzwi, a pasażerowie sprawdzają stan autobusu w bezpiecznej sekcji krytycznej.
* **Logowanie**: Kolorowe logi w terminalu oraz trwały zapis do pliku `symulacja.log` ze znacznikami czasu.

---

## 🛠️ Kompilacja i Uruchomienie

Projekt korzysta z narzędzia `make` do automatyzacji procesu budowania.

### Specyfikacja środowiska
* **System operacyjny:** Debian GNU/Linux 13 (trixie)
* **Jądro:** 6.12.63+deb13-amd64
* **Kompilator:** GCC 14.2.0
    * Flagi: `-Wall -pthread -D_POSIX_C_SOURCE=200809L`
* **Narzędzia:** `make`, `ipcs`, `ipcrm`

### Instrukcja

1. **Kompilacja projektu:**
   W katalogu z projektem wykonaj komendę:

   ```
   make 
   ```

    Spowoduje to utworzenie głównego pliku wykonywalnego `symulacja` oraz plików pomocniczych (`exe_bus`, `exe_passenger`, `exe_cashier`).


2. **Uruchomienie:**

    ```
    ./symulacja
    ```


3. **Czyszczenie (usunięcie plików binarnych i logów):**
    ```bash
    make clean
    ```



---

## 🎮 Sterowanie (Dyspozytor)

Podczas działania programu na pierwszym planie, użytkownik może sterować symulacją wpisując komendy w terminalu:

| Klawisz | Akcja | Opis |
| --- | --- | --- |
| **`1`** | **Wymuszony Odjazd** | Wysyła sygnał `SIGUSR1` do obecnego autobusu, nakazując mu natychmiastowe ruszenie w trasę (nawet jeśli nie jest pełny). |
| **`2`** | **Zamknięcie Dworca** | Wysyła sygnał `SIGUSR2`. Blokuje wejście nowych pasażerów, wymusza odjazd obecnego autobusu i czeka na zjazd wszystkich pojazdów do zajezdni. |
| **`Ctrl+C`** | **Awaryjne Stop** | Bezpieczne zatrzymanie symulacji i posprzątanie zasobów. |

Jeśli program został uruchomiony w tle, polecenie `kill <nazwa/numer sygnału> <PID procesu głównego>` zadziała analogicznie.

Można podać:

`-SIGUSR1` dla sygnału 1

`-SIGUSR2` dla sygnału 2

`-2` dla sygnału SIGINT

---

## 📄 Konfiguracja

Parametry symulacji są wczytywane z pliku `config.txt`. Zmiana tych wartości nie wymaga rekompilacji programu.

Przykładowa zawartość `config.txt`:

```ini
P=15            # Pojemność autobusu (liczba miejsc dla ludzi)
R=3             # Liczba miejsc na rowery
N=2             # Liczba autobusów krążących w systemie
T_POSTOJ=10     # Maksymalny czas postoju na przystanku (w sekundach)
```

---

## 📂 Struktura Plików

**Logika Główna:**
* `main.c` – Inicjalizacja, pętle generujące procesy, wątek sprzątający, obsługa `atexit`, logika **Dyspozytora**, monitorowanie, obsługa sygnałów systemowych.

**Aktorzy:**
* `exe_bus.c` – Logika autobusu (wjazd, postój, odjazd).
* `exe_passenger.c` – Logika pasażera (kasa, wsiadanie, obsługa wątku dziecka).
* `exe_cashier.c` – Logika kasjera (pętla obsługi komunikatów).

**Narzędzia:**
* `ipc_utils.c` – Wrappery na funkcje systemowe IPC (tworzenie/usuwanie zasobów).
* `config.c` – Parser pliku konfiguracyjnego.
* `logs.c` – Moduł logowania (plik + stdout).

**Nagłówki:**
* `common.h` – Wspólne definicje, stałe i struktury danych (`SharedData`, `BiletMsg`).
* `config.h`, `ipc_utils.h`, `logs.h` – pliki nagłówkowe zawierające deklaracje funkcji w analogicznych plikach źródłowych.




---

## ⚠️ Znane zachowania

* **Logi w tle**: Jeśli uruchomisz program w tle (`./symulacja &`), komunikaty nadal będą wypisywane na terminal. Aby temu zapobiec, użyj przekierowania: `./symulacja > /dev/null &` i śledź logi przez `tail -f symulacja.log`.
* **Natychmiastowy odjazd po wznowieniu (Ctrl+Z)**: Symulacja korzysta z zegara czasu rzeczywistego. Jeśli zatrzymasz symulację kombinacją Ctrl+Z, a następnie wznowisz ją komendą fg po czasie dłuższym niż T_POSTOJ, autobus odjedzie natychmiast.

---
## Testy
Wszelkie testy zostały przeprowadzone z zakomentowanymi funkcjami `sleep()` i `pause()`, chyba że podano inaczej.
### Test A: Rywalizacja o wjazd na przystanek
* **Cel:** Weryfikacja synchronizacji przy dostępie do zasobu rzadkiego `SEM_PRZYSTANEK` przez wiele procesów konkurujących.
* **Dane wejściowe:** P=20, R=5, N=50, T_POSTOJ=1
* **Przebieg:**
1. Uruchomienie symulacji z ogromną flotą autobusów.
2. Obserwacja logów pod kątem "nakładania się" autobusów.
* **Rezultat:** ✅ Pozytywny. Mimo ogromnego tłoku, w żadnym momencie w pamięci współdzielonej flaga autobus_obecny nie może zostać ustawiona przez dwa procesy jednocześnie. Każdy autobus musi stać w kolejce systemowej.

Przykładowe fragmenty logów:
```text
...
[23:15:29] [Autobus 1] Zgłasza gotowość na dworcu. Czekam na wjazd...
[23:15:29] [Autobus 14] Zgłasza gotowość na dworcu. Czekam na wjazd...
[23:15:29] [Autobus 36] Zgłasza gotowość na dworcu. Czekam na wjazd...
[23:15:29] [Autobus 13] Zgłasza gotowość na dworcu. Czekam na wjazd...
[23:15:29] [Autobus 23] Zgłasza gotowość na dworcu. Czekam na wjazd...
...
[23:15:29] [Autobus 1] Podstawiłem się. CZEKAM NA PASAŻERÓW (Czas: 1s)!
...
[23:15:30] [Autobus 1] Czas postoju minął.
[23:15:30] [Autobus 1] ODJAZD z 20 pasażerami (5 rowerów).
...
[23:15:30] [Autobus 2] Podstawiłem się. CZEKAM NA PASAŻERÓW (Czas: 1s)!
...
[23:17:32] [Autobus 2] Czas postoju minął.
...
[23:17:32] [Autobus 2] ODJAZD z 20 pasażerami (5 rowerów).
...
```

### Test B: Przepełnienie kolejki komunikatów
* **Cel:** Sprawdzenie stabilności kolejki komunikatów i mechanizmu priorytetów pod ekstremalnym obciążeniem.
* **Dane wejściowe:** P=20, R=5, N=10, T_POSTOJ=1; dodano sleep w procesie kasjera, aby kolejka mogła się przepełnić
* **Przebieg:**
1. Generator zalewa kolejkę żądań `msgid_req` setkami komunikatów.
2. W połowie zalewania seria pasażerów VIP zostaje wygenerowana.
* **Rezultat:** ✅ Pozytywny. Kasjer nie może się zawiesić. Mimo że w kolejce jest wiele zwykłych osób, Kasjer dzięki priorytetom musi najpierw wyłowić VIP-ów. Test sprawdza, czy bufor jądra dla kolejek nie został przekroczony.

Przykładowe fragmenty logów:
```text
...
[00:05:09] [Pasażer 45 (Zwykły)] Idę do kasy (PID: 1466724).
[00:05:09] [Pasażer 46 (Zwykły)] Idę do kasy (PID: 1466725).
[00:05:09] [Pasażer 49 (Zwykły)] Idę do kasy (PID: 1466728).
[00:05:09] [Pasażer 48 (Zwykły)] Idę do kasy (PID: 1466727).
[00:05:09] [Pasażer 50 (Zwykły)] Idę do kasy (PID: 1466729).
[00:05:09] [Pasażer 51 (VIP)] Mam karnet, omijam kolejkę do kasy. (PID: 1466730)
[00:05:09] [Pasażer 51 (VIP)] Przychodzę na przystanek.
[00:05:09] [Pasażer 51 (VIP)] Okazuję bilet i wsiadam! (Stan: 1/20, Rowery: 0/5)
[00:05:09] [Pasażer 52 (VIP)] Mam karnet, omijam kolejkę do kasy. (PID: 1466731)
[00:05:09] [Pasażer 52 (VIP)] Przychodzę na przystanek.
[00:05:09] [Pasażer 52 (VIP)] Okazuję bilet i wsiadam! (Stan: 2/20, Rowery: 0/5)
...
```

### Test C: Integralność limitu miejsc bez mechanizmu SEM_UNDO
* **Cel:** Weryfikacja poprawności manualnego uwalniania zasobów `SEM_LIMIT` podczas masowego wyjścia pasażerów. Test ma wykazać, że brak automatycznego czyszczenia jądra `SEM_UNDO` jest w pełni kompensowany przez poprawną obsługę sygnałów w kodzie pasażera.
* **Dane wejściowe:** P=20, R=5, N=1, T_POSTOJ=5; ustawienie `SEM_LIMIT` na 100; zablokowanie odjazdu autobusów poprzez zakomentowanie `odblokuj_semafor(semid, SEM_PRZYSTANEK)`, aby dworzec się zapełnił; zakomentowanie czyszczenia semaforów w funkcji `sprzatanie`
* **Przebieg:**
1. Generator tworzy pasażerów, aż dworzec osiągnie limit (100 osób). Generator zostaje zablokowany na semaforze `SEM_LIMIT`.
2. Weryfikacja stanu: ipcs -s -i [id_sem] wykazuje, że wartość semafora SEM_LIMIT wynosi 0.
3. Interwencja: Dyspozytor wysyła sygnał zamknięcia dworca (2 -> SIGUSR2).
4. Procesy pasażerów odbierają sygnał, wchodzą w handler `g_wyjscie`, wykonują funkcję `raportuj_wyjscie` i – co kluczowe – manualnie wołają `odblokuj_semafor_bez_undo(semid, SEM_LIMIT)`.
5. Zakończenie symulacji i ponowne sprawdzenie stanu semaforów.
* **Rezultat:** ✅ Pozytywny. Mimo braku `SEM_UNDO`, po zakończeniu procesów wartość semafora `SEM_LIMIT` wraca do pierwotnego stanu (100).

Przykładowe fragmenty logów i wyniki komend:
```text
...
[00:35:52] [Pasażer 122 (Zwykły)] Przychodzę na przystanek.
... komenda ipcs -s -i [id_sem]
n.sem.     wartość  oczek.n.   oczek.z.   pid       
6          0          1          0        1534430 
...
Bramy zamknięte.
Czekam na zjazd pozostałych autobusów...
...
--- RAPORT KOŃCOWY ---
Łącznie obsłużono pasażerów: 20
Wyszlo: 122 <- dzieci zwiększają liczbę osób (choć samych procesów jest nadal 100)
...
n.sem.     wartość  oczek.n.   oczek.z.   pid       
6          100          1          0        1534430 
...
```

### Test D: Atomowość wejścia i unikanie zakleszczeń
* **Cel:** Weryfikacja, czy para Opiekun + Dziecko jest traktowana jako niepodzielna jednostka zasobowa. Test ma wykazać, że procesy poprawnie sprawdzają warunki brzegowe w pamięci współdzielonej przed podjęciem próby zajęcia miejsca.
* **Dane wejściowe:** P=3, R=1, N=1, T_POSTOJ=5; ustawienie generatora, aby wygenerował 6 pasażerów w kombinacji (zwykły, opiekun, zwykły, zwykły, opiekun, zwykły)
* **Przebieg:**
1. Wchodzi jeden pasażer. Wolne miejsca: 2.
2. Na przystanku pierwszy w kolejce stoi Opiekun z dzieckiem (potrzebują 2 miejsc).
3. Opiekun sprawdza pamięć: (P - liczba_pasazerow) >= 2. Warunek spełniony -> para wsiada. Wolne miejsca: 0.
4. Autobus odjeżdża, wraca.
5. Wchodzi dwóch pasażerów Zwykłych. Wolne miejsca: 1.
6. W kolejce czeka kolejny Opiekun oraz inny pasażer.
7. Opiekun sprawdza pamięć: 1 < 2. Opiekun rezygnuje z wejścia do tego autobusu i zwalnia miejsce kolejnemu pasażerowi.
* **Rezultat:** ✅ Pozytywny. Pasażerowie nie rozdzielają się. Opiekunowie potrafią "odpuścić" zbyt pełny autobus, pozwalając na wejście pojedynczym osobom, co zapobiega zakleszczeniu kolejki.

Przykładowe fragmenty logów:
```text
...
[01:03:13] [Pasażer 1 (Zwykły)] Przychodzę na przystanek.
[01:03:13] [Pasażer 1 (Zwykły)] Okazuję bilet i wsiadam! (Stan: 1/3, Rowery: 0/1)
...
[01:03:14] [Pasażer 2 (Opiekun)] Przychodzę na przystanek.
[01:03:14] [Pasażer 2 (Opiekun)] Okazuję bilet i wsiadam! (Stan: 3/3, Rowery: 0/1)
[01:03:14] [Opiekun 2] Wprowadzam dziecko do autobusu.
...
[01:03:15] [Pasażer 3 (Zwykły)] Przychodzę na przystanek.
...
[01:03:16] [Pasażer 4 (Zwykły)] Przychodzę na przystanek.
...
[01:03:17] [Pasażer 5 (Opiekun)] Przychodzę na przystanek.
...
[01:03:18] [Pasażer 6 (Zwykły)] Przychodzę na przystanek.
...
[01:03:17] [Autobus 1] ODJAZD z 3 pasażerami (0 rowerów).
...
[01:03:31] [Autobus 1] WRÓCIŁ Z TRASY po 14 s.
[01:03:31] [Autobus 1] Podstawiłem się. CZEKAM NA PASAŻERÓW (Czas: 5s)!
[01:03:31] [Pasażer 3 (Zwykły)] Okazuję bilet i wsiadam! (Stan: 1/3, Rowery: 0/1)
[01:03:31] [Pasażer 4 (Zwykły)] Okazuję bilet i wsiadam! (Stan: 2/3, Rowery: 0/1)
[01:03:31] [Pasażer 6 (Zwykły)] Okazuję bilet i wsiadam! (Stan: 3/3, Rowery: 0/1)
...
```

### Test E: Interwencja Dyspozytora (Sygnał)
* **Cel:** Weryfikacja mechanizmu kaskadowego kończenia procesów pod dużym obciążeniem. Sprawdzenie, czy sygnał `SIGUSR2` dociera do wszystkich procesów potomnych i czy każdy z nich poprawnie odłącza się od pamięci współdzielonej `shmdt` przed zakończeniem pracy.
* **Dane wejściowe:** P=20, R=5, N=1, T_POSTOJ=15
* **Przebieg:**
1. Uruchomienie symulacji i doprowadzenie do stanu, w którym w systemie żyje kilkaset procesów pasażerów.
2. Wysłanie sygnału ewakuacji przez Dyspozytora (Klawisz 2 -> SIGUSR2).
3. Obserwacja kaskady: Proces główny (Main) rozsyła sygnał do grup procesów.
4. Każdy proces musi przerwać aktualną operację i zakończyć działanie.
5. Weryfikacja końcowa: Użycie komend systemowych do sprawdzenia, czy system "posprzątał".
* **Rezultat:** ✅ Pozytywny. Wszystkie procesy potomne znikają z listy procesów (ps). Pamięć współdzielona i semafory zostają usunięte przez proces Main. Brak procesów-zombie.

Przykładowe fragmenty logów i wyniki komend:
```text
...
[01:15:14] [Pasażer 3017 (Rower)] Przychodzę na przystanek.
...
[01:15:18] Bramy zamknięte.
[01:15:18] Wymuszam odjazd obecnego autobusu (PID 1581366)
[01:15:18] Czekam na zjazd pozostałych autobusów...
...
[01:15:18] [Autobus 1] Otrzymano nakaz natychmiastowego odjazdu!
[01:15:18] [Autobus 1] ODJAZD z 20 pasażerami (5 rowerów).
[01:15:30] [Autobus 1] WRÓCIŁ Z TRASY po 12 s.
[01:15:30] [Autobus 1] Zjazd do zajezdni (Koniec pracy).
[01:15:30] [MAIN] Wszystkie autobusy zjechały.
[01:15:30] --- RAPORT KOŃCOWY ---
[01:15:30] Łącznie obsłużono pasażerów: 20
[01:15:30] Wyszlo: 3545
[01:15:30] [SYSTEM] Zasoby posprzątane. Koniec.
...
ps aux | grep exe_ | wc -l
1 <- sam grep
...
ipcs -s | grep 1000
(nic) <- wszystkie semafory usunięte
...
ipcs -m | grep 1000
(nic) <- pamięć dzielona zwolniona
...
ipcs -q | grep 1000
(nic) <- wszystkie kolejki komunikatów usunięte
...
```

---
## Linki do kluczowych fragmentów

### a. Tworzenie i obsługa plików
Wykorzystano bibliotekę standardową (nakładka na syscall open/write) do logowania zdarzeń.
   
https://github.com/Bar-t-e-k/Symulacja_autobusu_podmiejskiego/blob/2c46a2e24341c8337f216a7229a4ffc224fc4dc0/logs.c#L24-L37

### b. Tworzenie procesów
Podstawa architektury. Proces główny tworzy procesy potomne, które zmieniają swój obraz pamięci (exec).
 
Kasjer: 
https://github.com/Bar-t-e-k/Symulacja_autobusu_podmiejskiego/blob/3aae22b09b5f9201e8cdca44207118b0a8ed0167/main.c#L135-L150
Autobusy: 
https://github.com/Bar-t-e-k/Symulacja_autobusu_podmiejskiego/blob/3aae22b09b5f9201e8cdca44207118b0a8ed0167/main.c#L153-L171
Pasażerowie: 
https://github.com/Bar-t-e-k/Symulacja_autobusu_podmiejskiego/blob/3aae22b09b5f9201e8cdca44207118b0a8ed0167/main.c#L175-L233

### c. Obsługa sygnałów
Reakcja na interwencję Dyspozytora oraz bezpieczne zamykanie.

Dyspozytor: 
https://github.com/Bar-t-e-k/Symulacja_autobusu_podmiejskiego/blob/3aae22b09b5f9201e8cdca44207118b0a8ed0167/main.c#L237-L274
Ctrl + C: 
https://github.com/Bar-t-e-k/Symulacja_autobusu_podmiejskiego/blob/3aae22b09b5f9201e8cdca44207118b0a8ed0167/main.c#L279-L282
Sygnał 1: 
https://github.com/Bar-t-e-k/Symulacja_autobusu_podmiejskiego/blob/3aae22b09b5f9201e8cdca44207118b0a8ed0167/main.c#L288-L299
Sygnał 2: 
https://github.com/Bar-t-e-k/Symulacja_autobusu_podmiejskiego/blob/3aae22b09b5f9201e8cdca44207118b0a8ed0167/main.c#L301-L320

### e. Synchronizacja procesów
Wykorzystano semafory do ochrony zasobów i blokowania wejścia do drzwi.

https://github.com/Bar-t-e-k/Symulacja_autobusu_podmiejskiego/blob/3aae22b09b5f9201e8cdca44207118b0a8ed0167/ipc_utils.c#L61-L178

### g. Segmenty pamięci dzielonej
Współdzielenie stanu dworca między procesami.

https://github.com/Bar-t-e-k/Symulacja_autobusu_podmiejskiego/blob/3aae22b09b5f9201e8cdca44207118b0a8ed0167/ipc_utils.c#L14-L57

### h. Kolejki komunikatów
Komunikacja między Kasjerem a Pasażerami

https://github.com/Bar-t-e-k/Symulacja_autobusu_podmiejskiego/blob/3aae22b09b5f9201e8cdca44207118b0a8ed0167/ipc_utils.c#L182-L228

### i. Konfiguracja i walidacja
Dane wczytywane z pliku config.txt.

https://github.com/Bar-t-e-k/Symulacja_autobusu_podmiejskiego/blob/3aae22b09b5f9201e8cdca44207118b0a8ed0167/config.c#L7-L72

### j. Wykorzystanie wątków
Dziecko jest tworzone jako wątek wewnątrz opiekuna.

https://github.com/Bar-t-e-k/Symulacja_autobusu_podmiejskiego/blob/3aae22b09b5f9201e8cdca44207118b0a8ed0167/exe_passenger.c#L23-L39

### k. Funkcje aktorów

Kasjer: 
https://github.com/Bar-t-e-k/Symulacja_autobusu_podmiejskiego/blob/3aae22b09b5f9201e8cdca44207118b0a8ed0167/exe_cashier.c#L13-L48
Autobus: 
https://github.com/Bar-t-e-k/Symulacja_autobusu_podmiejskiego/blob/3aae22b09b5f9201e8cdca44207118b0a8ed0167/exe_bus.c#L30-L214
Pasażer: 
https://github.com/Bar-t-e-k/Symulacja_autobusu_podmiejskiego/blob/3aae22b09b5f9201e8cdca44207118b0a8ed0167/exe_passenger.c#L41-L200

### l. Obsługa błędów i logi
Wykorzystano własne funkcje do zapisywania logów i obsługi błędów

https://github.com/Bar-t-e-k/Symulacja_autobusu_podmiejskiego/blob/86c3ae1d4f3778efd698ad36cdce002232e9db5e/logs.c#L11-L45

---
**Autor:** Bartłomiej Zięcina
