# 🚌 Symulator Dworca Autobusowego

Projekt realizuje zaawansowaną symulację dworca autobusowego w środowisku Linux, wykorzystując mechanizmy wieloprocesowości oraz komunikacji międzyprocesowej.

Głównym celem projektu jest demonstracja synchronizacji procesów, zarządzania zasobami dzielonymi oraz obsługi sygnałów w systemie operacyjnym.

---

## 🚀 Funkcjonalności

### 1. Architektura Wieloprocesowa
Symulacja wykorzystuje funkcje `fork()` oraz rodzinę funkcji `exec()` do tworzenia niezależnych procesów dla każdego aktora systemu:
* **Główny Zarządca (`main`)**: Inicjuje zasoby, tworzy procesy potomne i sprząta po zakończeniu oraz **pełni rolę Dyspozytora** (obsługuje sterowanie z klawiatury).
* **Autobusy**: Niezależne procesy realizujące kursy, zabierające pasażerów i obsługujące limity miejsc.
* **Pasażerowie**: Procesy symulujące zachowanie ludzi (kupno biletu, oczekiwanie, wejście).
* **Kasjer**: Proces obsługujący kolejkę komunikatów z zapytaniami o bilety.

### 2. Typy Pasażerów i Logika Biznesowa
System obsługuje różne typy pasażerów ze specyficznymi zachowaniami:
* **🟢 Pasażer Zwykły**: Musi kupić bilet w kasie, a następnie czeka w kolejce do autobusu.
* **🟣 VIP**: Posiada priorytet – omija kolejkę do kasy oraz ma pierwszeństwo wejścia do autobusu przed zwykłymi pasażerami.
* **🔵 Rodzina (Opiekun + Dziecko)**: Zaawansowany mechanizm synchronizacji. Opiekun wchodzi tylko wtedy, gdy są **dwa** wolne miejsca. Dziecko czeka na sygnał od opiekuna. Autobus nie odjeżdża, jeśli opiekun wszedł, a dziecko jeszcze nie.
* **🚲 Rowerzysta**: Zajmuje miejsce pasażerskie ORAZ limitowane miejsce na rower.

### 3. Mechanizmy IPC
* **Pamięć Dzielona (Shared Memory)**: Przechowuje globalny stan dworca (m.in. liczniki pasażerów, flagi otwarcia, PID obecnego autobusu).
* **Semafory (Semaphores)**:
    * `SEM_MUTEX`: Gwarantuje wyłączny dostęp do pamięci dzielonej (sekcja krytyczna).
    * `SEM_DRZWI`: Ograniczają przepustowość wejścia do autobusu.
* **Kolejki Komunikatów (Message Queues)**:
    * Komunikacja `Pasażer -> Kasjer` (symulacja zakupu biletu).
    * Komunikacja `Pasażer -> Kierowca` (kontrola biletów przy wejściu).
    * Synchronizacja `Opiekun <-> Dziecko`.

### 4. Bezpieczeństwo i Logowanie
* **Graceful Shutdown**: System gwarantuje usunięcie zasobów IPC (pamięć, semafory) niezależnie od sposobu zakończenia programu (sygnał `SIGINT`, błąd, czy normalne zakończenie) dzięki `atexit()`.
* **Deadlock Prevention**: Zastosowanie timeoutów w oczekiwaniu na komunikaty zapobiega zakleszczeniom (np. gdy kierowca odrzuci opiekuna, dziecko nie czeka w nieskończoność).
* **Logowanie**: Kolorowe logi w terminalu oraz trwały zapis do pliku `symulacja.log` ze znacznikami czasu.

---

## 🛠️ Kompilacja i Uruchomienie

Projekt korzysta z narzędzia `make` do automatyzacji procesu budowania.

### Wymagania
* System operacyjny: Linux
* Kompilator: GCC
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
* `main.c` – Inicjalizacja, pętle generujące procesy, obsługa `atexit`, logika **Dyspozytora**.
* `actors.c` – Implementacja funkcji aktorów (`kasjer_run`, `pasazer_run`, `autobus_run`).
* `signals.c` – Obsługa sygnałów systemowych.


**Narzędzia:**
* `ipc_utils.c` – Wrappery na funkcje systemowe IPC (tworzenie/usuwanie zasobów).
* `config.c` – Parser pliku konfiguracyjnego.
* `logs.c` – Moduł logowania (plik + stdout).


**Nagłówki:**
* `common.h` – Wspólne definicje, stałe i struktury danych (`SharedData`, `BiletMsg`).
* `actors.h`, `config.h`, `ipc_utils.h`, `logs.h`, `signals.h` – pliki nagłówkowe zawierające deklaracje funkcji w analogicznych plikach źródłowych.


**Wrappery Exec:**
* `exe_bus.c`, `exe_passenger.c`, `exe_cashier.c` – Programy uruchamiane przez `exec()`, które wywołują właściwą logikę z pliku `actors`.



---

## ⚠️ Znane zachowania

* **Procesy Zombie**: Podczas działania symulacji na liście procesów mogą pojawiać się procesy **Zombie**. Jest to normalne zachowanie przy intensywnym tworzeniu procesów potomnych, które nie są natychmiast zbierane przez `wait()`. Są one automatycznie sprzątane przez system przy zamknięciu programu.
* **Logi w tle**: Jeśli uruchomisz program w tle (`./symulacja &`), komunikaty nadal będą wypisywane na terminal. Aby temu zapobiec, użyj przekierowania: `./symulacja > /dev/null &` i śledź logi przez `tail -f symulacja.log`.

---
## Testy

### Test A: Standardowy cykl przewozu
* **Scenariusz:** Pasażerowie przychodzą, kupują bilety, zapełniają autobus. Po upływie czasu `T_POSTOJ` autobus odjeżdża.
* **Weryfikacja techniczna:** Pasażer wysyła zapytanie do Kasjera na `KANAL_ZAPYTAN`. Autobus w pętli sprawdza czas systemowy `time()`. Po przekroczeniu limitu czasu pętla załadunku zostaje przerwana.
* **Rezultat:** ✅ Pozytywny. Logi potwierdzają sekwencję: Zakup -> Wejście -> Odjazd po czasie.

### Test B: Przepełnienie i limit rowerów
* **Scenariusz:** Liczba chętnych przekracza limit `P`, a liczba rowerzystów przekracza limit `R`.
* **Weryfikacja techniczna:** Przed wejściem sprawdzany jest warunek w Pamięci Dzielonej: `if (liczba_pasazerow >= P)` oraz `if (typ == ROWER && liczba_rowerow >= R)`. Dostęp do liczników chroni semafor `MUTEX`. Jeśli warunek jest niespełniony, Kierowca odsyła komunikat odmowny (`-1`).
* **Rezultat:** ✅ Pozytywny. Pasażerowie nadmiarowi otrzymują odmowę i czekają na kolejny autobus.

### Test C: Obsługa priorytetów (VIP)
* **Scenariusz:** W kolejce czekają pasażerowie Zwykli. Pojawia się VIP.
* **Weryfikacja techniczna:** VIP inkrementuje licznik `liczba_vip_oczekujacych` w pamięci dzielonej. Pasażerowie zwykli w pętli oczekiwania sprawdzają ten licznik. Jeśli jest `> 0`, wykonują `usleep` i zwalniają semafor drzwi, przepuszczając VIP-a.
* **Rezultat:** ✅ Pozytywny. VIP wchodzi do autobusu natychmiast, z pominięciem kolejki.

### Test D: Zależność Dziecko-Opiekun
* **Scenariusz:** Do wejścia podchodzi Opiekun z Dzieckiem.
* **Weryfikacja techniczna:**
    1. Opiekun sprawdza dostępność `P-2` miejsc.
    2. Po wejściu Opiekun wysyła wiadomość IPC do Dziecka.
    3. Autobus ustawia flagę `oczekuje_na_dziecko` i blokuje odjazd (nawet po upływie czasu) do momentu otrzymania potwierdzenia wejścia Dziecka.
* **Rezultat:** ✅ Pozytywny. Autobus zaczekał na dziecko mimo upływu czasu postoju.

### Test E: Interwencja Dyspozytora (Sygnały)
* **Scenariusz:** Użytkownik naciska klawisz `1` podczas załadunku.
* **Weryfikacja techniczna:** Proces `main` wysyła sygnał `SIGUSR1` do procesu autobusu. Handler sygnału w autobusie ustawia zmienną `volatile sig_atomic_t wymuszony_odjazd = 1`, co natychmiast przerywa pętlę załadunku instrukcją `break`.
* **Rezultat:** ✅ Pozytywny. Autobus odjechał natychmiast, nie czekając na pełny załadunek ani upływ czasu.

---
**Autor:** Bartłomiej Zięcina
