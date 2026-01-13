# 🚌 Symulator Dworca Autobusowego

Projekt realizuje zaawansowaną symulację dworca autobusowego w środowisku Linux, wykorzystując mechanizmy wieloprocesowości, wielowątkowości oraz synchronizacji zasobów.

Głównym celem projektu jest demonstracja problemów współbieżności: wykluczania wzajemnego, synchronizacji procesów, obsługi sygnałów oraz zapobiegania zakleszczeniom.

---

## 🚀 Funkcjonalności

### 1. Architektura Wieloprocesowa
Symulacja wykorzystuje funkcje `fork()` oraz rodzinę funkcji `execlp()` do tworzenia niezależnych procesów dla każdego aktora systemu:
* **Główny Zarządca (`main`)**: Inicjuje zasoby, tworzy procesy potomne, monitoruje stan symulacji, sprząta po zakończeniu oraz **pełni rolę Dyspozytora** (obsługuje sterowanie z klawiatury).
* **Autobusy**: Niezależne procesy realizujące kursy, zabierające pasażerów i obsługujące limity miejsc.
* **Pasażerowie**: Procesy symulujące zachowanie ludzi (kupno biletu, oczekiwanie, wejście).
* **Kasjer**: Proces obsługujący kolejkę komunikatów z zapytaniami o bilety.

### 2. Typy Pasażerów i Logika Biznesowa
System obsługuje różne typy pasażerów ze specyficznymi zachowaniami:
* **🟢 Pasażer Zwykły**: Musi kupić bilet w kasie, a następnie czeka w kolejce do autobusu.
* **🟣 VIP**: Posiada priorytet – omija kolejkę do kasy oraz ma pierwszeństwo wejścia do autobusu przed zwykłymi pasażerami.
* **🔵 Rodzina (Opiekun + Dziecko)**: Zaimplementowana jako jeden proces z wątkiem. Proces Opiekuna sprawdza dostępność dwóch miejsc. Dziecko jest realizowane jako wątek `pthread`, który towarzyszy procesowi rodzica.
* **🚲 Rowerzysta**: Zajmuje miejsce pasażerskie ORAZ limitowane miejsce na rower. Używa osobnego wejścia.

### 3. Mechanizmy IPC
* **Pamięć Dzielona (Shared Memory)**: Przechowuje globalny stan dworca (m.in. liczniki pasażerów, flagi otwarcia, PID obecnego autobusu).
* **Semafory (Semaphores)**:
    * `SEM_MUTEX`: Gwarantuje wyłączny dostęp do pamięci dzielonej (sekcja krytyczna).
    * `SEM_DRZWI_PAS`: Ogranicza przepustowość wejścia pasażerskiego.
    * `SEM_DRZWI_ROW`: Ogranicza przepustowość wejścia dla rowerów.
* **Kolejki Komunikatów (Message Queues)**:
    * Komunikacja `Pasażer -> Kasjer` (symulacja zakupu biletu).

### 4. Bezpieczeństwo i Logowanie
* **Graceful Shutdown**: System gwarantuje usunięcie zasobów IPC (pamięć, semafory) niezależnie od sposobu zakończenia programu (sygnał `SIGINT`, błąd, czy normalne zakończenie) dzięki `atexit()`.
* **Deadlock Prevention**: Autobus nie blokuje semaforów drzwi, a pasażerowie sprawdzają stan autobusu w bezpiecznej sekcji krytycznej.
* **Logowanie**: Kolorowe logi w terminalu oraz trwały zapis do pliku `symulacja.log` ze znacznikami czasu.

---

## 🛠️ Kompilacja i Uruchomienie

Projekt korzysta z narzędzia `make` do automatyzacji procesu budowania.

### Wymagania
* System operacyjny: Linux
* Kompilator: GCC (z obsługą `pthread`)
* Biblioteki standardowe C

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
L_PASAZEROW=30  # Limit pasażerów do obsłużenia podczas trwania symulacji (warunek zakończenia symulacji)
```

---

## 📂 Struktura Plików

**Logika Główna:**
* `main.c` – Inicjalizacja, pętle generujące procesy, obsługa `atexit`, logika **Dyspozytora**, monitorowanie.
* `signals.c` – Obsługa sygnałów systemowych.

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
* `config.h`, `ipc_utils.h`, `logs.h`, `signals.h` – pliki nagłówkowe zawierające deklaracje funkcji w analogicznych plikach źródłowych.




---

## ⚠️ Znane zachowania

* **Procesy Zombie**: Podczas działania symulacji na liście procesów mogą pojawiać się procesy **Zombie**. Jest to normalne zachowanie przy intensywnym tworzeniu procesów potomnych, które nie są natychmiast zbierane przez `wait()`. Są one automatycznie sprzątane przez system przy zamknięciu programu.
* **Logi w tle**: Jeśli uruchomisz program w tle (`./symulacja &`), komunikaty nadal będą wypisywane na terminal. Aby temu zapobiec, użyj przekierowania: `./symulacja > /dev/null &` i śledź logi przez `tail -f symulacja.log`.
* **Natychmiastowy odjazd po wznowieniu (Ctrl+Z)**: Symulacja korzysta z zegara czasu rzeczywistego. Jeśli zatrzymasz symulację kombinacją Ctrl+Z, a następnie wznowisz ją komendą fg po czasie dłuższym niż T_POSTOJ, autobus odjedzie natychmiast.

---
## Testy

### Test A: Standardowy cykl przewozu
* **Scenariusz:** Pasażerowie przychodzą, kupują bilety, zapełniają autobus. Po upływie czasu `T_POSTOJ` autobus odjeżdża.
* **Weryfikacja techniczna:** Pasażer wysyła zapytanie do Kasjera na `KANAL_ZAPYTAN`. Autobus w pętli sprawdza czas systemowy `time()`. Po przekroczeniu limitu czasu pętla załadunku zostaje przerwana.
* **Rezultat:** ✅ Pozytywny. Logi potwierdzają sekwencję: Zakup -> Wejście -> Odjazd po czasie.

### Test B: Przepełnienie i limit rowerów
* **Scenariusz:** Liczba chętnych przekracza limit `P`, a liczba rowerzystów przekracza limit `R`.
* **Weryfikacja techniczna:** Przed wejściem sprawdzany jest warunek w Pamięci Dzielonej: `liczba_pasazerow + miejsce_potrzebne <= P` oraz `liczba_rowerow + rower_potrzebny <= R`. Dostęp do liczników chroni semafor `MUTEX`.
* **Rezultat:** ✅ Pozytywny. Pasażerowie nadmiarowi otrzymują odmowę i czekają na kolejny autobus.

### Test C: Obsługa priorytetów (VIP)
* **Scenariusz:** W kolejce czekają pasażerowie Zwykli. Pojawia się VIP.
* **Weryfikacja techniczna:** VIP inkrementuje licznik `liczba_vip_oczekujacych` w pamięci dzielonej. Pasażerowie zwykli w pętli oczekiwania sprawdzają ten licznik. Jeśli jest `> 0`, wykonują `usleep` i zwalniają semafor drzwi, przepuszczając VIP-a.
* **Rezultat:** ✅ Pozytywny. VIP wchodzi do autobusu natychmiast, z pominięciem kolejki.

### Test D: Zależność Dziecko-Opiekun
* **Scenariusz:** Do wejścia podchodzi Opiekun z Dzieckiem.
* **Weryfikacja techniczna:**
    1. Opiekun sprawdza dostępność `P-2` miejsc.
    2. Jeśli są wolne, inkrementuje licznik pasażerów o 2 (za siebie i dziecko) w jednej transakcji atomowej.
    3. Wątek dziecka (wewnątrz procesu Opiekuna) jest synchronizowany za pomocą pthread_cond i kończy podróż razem z rodzicem.
* **Rezultat:** ✅ Pozytywny. Opiekun zajmuje poprawną liczbę miejsc, a dziecko "podróżuje" razem z nim bez blokowania zasobów zewnętrznych.

### Test E: Interwencja Dyspozytora (Sygnały)
* **Scenariusz:** Użytkownik naciska klawisz `1` podczas załadunku.
* **Weryfikacja techniczna:** Proces `main` wysyła sygnał `SIGUSR1` do procesu autobusu. Handler sygnału w autobusie ustawia zmienną `volatile sig_atomic_t wymuszony_odjazd = 1`, co natychmiast przerywa pętlę załadunku instrukcją `break`.
* **Rezultat:** ✅ Pozytywny. Autobus odjechał natychmiast, nie czekając na pełny załadunek ani upływ czasu.

---
## Linki do kluczowych fragmentów

### a. Tworzenie i obsługa plików
Wykorzystano bibliotekę standardową (nakładka na syscall open/write) do logowania zdarzeń.
   
https://github.com/Bar-t-e-k/Symulacja_autobusu_podmiejskiego/blob/2c46a2e24341c8337f216a7229a4ffc224fc4dc0/logs.c#L24-L37

### b. Tworzenie procesów
Podstawa architektury. Proces główny tworzy procesy potomne, które zmieniają swój obraz pamięci (exec).
 
Kasjer: 
https://github.com/Bar-t-e-k/Symulacja_autobusu_podmiejskiego/blob/86c3ae1d4f3778efd698ad36cdce002232e9db5e/main.c#L103-L117
Autobusy: 
https://github.com/Bar-t-e-k/Symulacja_autobusu_podmiejskiego/blob/86c3ae1d4f3778efd698ad36cdce002232e9db5e/main.c#L120-L137
Pasażerowie: 
https://github.com/Bar-t-e-k/Symulacja_autobusu_podmiejskiego/blob/86c3ae1d4f3778efd698ad36cdce002232e9db5e/main.c#L140-L185

### c. Obsługa sygnałów
Reakcja na interwencję Dyspozytora oraz bezpieczne zamykanie.

Dyspozytor: 
https://github.com/Bar-t-e-k/Symulacja_autobusu_podmiejskiego/blob/86c3ae1d4f3778efd698ad36cdce002232e9db5e/main.c#L188-L224
Ctrl + C: 
https://github.com/Bar-t-e-k/Symulacja_autobusu_podmiejskiego/blob/2c46a2e24341c8337f216a7229a4ffc224fc4dc0/signals.c#L18-L22
Sygnał 1: 
https://github.com/Bar-t-e-k/Symulacja_autobusu_podmiejskiego/blob/2c46a2e24341c8337f216a7229a4ffc224fc4dc0/signals.c#L26-L41
Sygnał 2: 
https://github.com/Bar-t-e-k/Symulacja_autobusu_podmiejskiego/blob/2c46a2e24341c8337f216a7229a4ffc224fc4dc0/signals.c#L45-L65

### e. Synchronizacja procesów
Wykorzystano semafory do ochrony zasobów i blokowania wejścia do drzwi.

https://github.com/Bar-t-e-k/Symulacja_autobusu_podmiejskiego/blob/86c3ae1d4f3778efd698ad36cdce002232e9db5e/ipc_utils.c#L56-L113

### g. Segmenty pamięci dzielonej
Współdzielenie stanu dworca między procesami.

https://github.com/Bar-t-e-k/Symulacja_autobusu_podmiejskiego/blob/86c3ae1d4f3778efd698ad36cdce002232e9db5e/ipc_utils.c#L14-L52

### h. Kolejki komunikatów
Komunikacja między Kasjerem a Pasażerami

https://github.com/Bar-t-e-k/Symulacja_autobusu_podmiejskiego/blob/86c3ae1d4f3778efd698ad36cdce002232e9db5e/ipc_utils.c#L117-L156

### i. Konfiguracja i walidacja
Dane wczytywane z pliku config.txt.

https://github.com/Bar-t-e-k/Symulacja_autobusu_podmiejskiego/blob/2c46a2e24341c8337f216a7229a4ffc224fc4dc0/config.c#L8-L75

### j. Wykorzystanie wątków
Dziecko jest tworzone jako wątek wewnątrz opiekuna.

https://github.com/Bar-t-e-k/Symulacja_autobusu_podmiejskiego/blob/86c3ae1d4f3778efd698ad36cdce002232e9db5e/exe_passenger.c#L24-L38

### k. Funkcje aktorów

Kasjer: 
https://github.com/Bar-t-e-k/Symulacja_autobusu_podmiejskiego/blob/86c3ae1d4f3778efd698ad36cdce002232e9db5e/exe_cashier.c#L14-L32
Autobus: 
https://github.com/Bar-t-e-k/Symulacja_autobusu_podmiejskiego/blob/86c3ae1d4f3778efd698ad36cdce002232e9db5e/exe_bus.c#L22-L145
Pasażer: 
https://github.com/Bar-t-e-k/Symulacja_autobusu_podmiejskiego/blob/86c3ae1d4f3778efd698ad36cdce002232e9db5e/exe_passenger.c#L41-L202

### l. Obsługa błędów i logi
Wykorzystano własne funkcje do zapisywania logów i obsługi błędów

https://github.com/Bar-t-e-k/Symulacja_autobusu_podmiejskiego/blob/86c3ae1d4f3778efd698ad36cdce002232e9db5e/logs.c#L11-L45

---
**Autor:** Bartłomiej Zięcina
