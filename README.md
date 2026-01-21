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
    * `SEM_LIMIT`: Ogranicza maksymalną liczbę pasażerów przebywających jednocześnie na dworcu (zapobiega przepełnieniu systemu).
    * `SEM_WSIADL`: Sygnał zwrotny od pasażera do autobusu, potwierdzający zakończenie procedury wsiadania.
* **Kolejki Komunikatów (Message Queues)**:
    * Komunikacja `Pasażer <-> Kasjer` (symulacja zakupu biletu) (2 kolejki do obsługi w dwie strony).

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
```

---

## 📂 Struktura Plików

**Logika Główna:**
* `main.c` – Inicjalizacja, pętle generujące procesy, obsługa `atexit`, logika **Dyspozytora**, monitorowanie, obsługa sygnałów systemowych.

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

* **Procesy Zombie**: Podczas działania symulacji na liście procesów mogą pojawiać się procesy **Zombie**. Jest to normalne zachowanie przy intensywnym tworzeniu procesów potomnych, które nie są natychmiast zbierane przez `wait()`. Są one automatycznie sprzątane przez system przy zamknięciu programu.
* **Logi w tle**: Jeśli uruchomisz program w tle (`./symulacja &`), komunikaty nadal będą wypisywane na terminal. Aby temu zapobiec, użyj przekierowania: `./symulacja > /dev/null &` i śledź logi przez `tail -f symulacja.log`.
* **Natychmiastowy odjazd po wznowieniu (Ctrl+Z)**: Symulacja korzysta z zegara czasu rzeczywistego. Jeśli zatrzymasz symulację kombinacją Ctrl+Z, a następnie wznowisz ją komendą fg po czasie dłuższym niż T_POSTOJ, autobus odjedzie natychmiast.

---
## Testy

### Test A: Standardowy cykl przewozu
* **Scenariusz:** Pasażerowie przychodzą, kupują bilety, zapełniają autobus. Po upływie czasu `T_POSTOJ` autobus odjeżdża.
* **Dane wejściowe:** P=20, R=5, N=1, T_POSTOJ=5
* **Przebieg:**
1. Autobus zgłasza gotowość i zajmuje przystanek (semafor `SEM_PRZYSTANEK`) i nastawia budzik systemowy.
2. Proces autobusu przechodzi w stan uśpienia na semaforze  `SEM_KTOS_CZEKA`.  
3. Każdy pasażer po zakupie biletu podnosi semafor dzwonka, wybudzając autobus do obsługi wejścia.
4. Autobus wpuszcza pasażera, aktualizuje liczniki i ponownie zasypia, czekając na kolejnych chętnych.
5. Po upływie 5 sekund sygnał `SIGALRM` przerywa oczekiwanie na semaforze, co kończy fazę załadunku.
6. Autobus zwalnia przystanek i odjeżdża w trasę.
* **Rezultat:** ✅ Pozytywny. Logi potwierdzają sekwencję: Zakup -> Wejście -> Odjazd po czasie.

Przykładowe fragmenty logów:
```text
...
[21:18:16] [Autobus 1] Zgłasza gotowość na dworcu. Czekam na wjazd...
[21:18:16] [Autobus 1] Podstawiłem się. CZEKAM NA PASAŻERÓW (Czas: 5s)!
...
[21:18:16] [Pasażer 11 (Zwykły)] Idę do kasy (PID: 1234192).
[21:18:16] [Pasażer 1 (Rower)] Idę do kasy (PID: 1234182).
[21:18:16] [Pasażer 3 (Zwykły)] Idę do kasy (PID: 1234184).
...
[21:18:16] [Pasażer 13 (Zwykły)] Przychodzę na przystanek.
[21:18:16] [Pasażer 11 (Zwykły)] Przychodzę na przystanek.
[21:18:16] [Pasażer 1 (Rower)] Przychodzę na przystanek.
...
[21:18:16] [Pasażer 8 (VIP)] Okazuję bilet i wsiadam! (Stan: 1/20, Rowery: 0/5)
[21:18:16] [Pasażer 1 (Rower)] Okazuję bilet i wsiadam! (Stan: 2/20, Rowery: 1/5)
...
[21:18:21] [Autobus 1] Czas postoju minął.
[21:18:21] [Autobus 1] ODJAZD z 20 pasażerami (5 rowerów).
[21:18:34] [Autobus 1] WRÓCIŁ Z TRASY po 13 s.
```

### Test B: Przepełnienie i limit rowerów
* **Scenariusz:** Liczba chętnych przekracza limit `P`, a liczba rowerzystów przekracza limit `R`.
* **Dane wejściowe:** P=20, R=5, N=1, T_POSTOJ=5
* **Przebieg:**
1. Następuje zapełnienie limitu rowerów: licznik rowerów osiąga 5/5. Kolejni pasażerowie wsiadają już bez rowerów, mimo że wciąż są wolne miejsca ogólne.
2. Zapełnienie limitu miejsc: licznik osiąga stan 20/20. Od tego momentu proces autobusu ignoruje dzwonki pasażerów i czeka na sygnał odjazdu.
3. Po powrocie autobus zaczyna wpuszczać kolejnych pasażerów.
* **Rezultat:** ✅ Pozytywny. Pasażerowie nadmiarowi otrzymują odmowę i czekają na kolejny autobus.

Przykładowe fragmenty logów:
```text
...
[21:25:55] [Pasażer 30 (VIP)] Okazuję bilet i wsiadam! (Stan: 1/20, Rowery: 0/5) 
[21:25:55] [Pasażer 43 (Rower)] Okazuję bilet i wsiadam! (Stan: 2/20, Rowery: 1/5) 
[21:25:55] [Pasażer 32 (Rower)] Okazuję bilet i wsiadam! (Stan: 3/20, Rowery: 2/5) 
[21:25:55] [Pasażer 27 (Rower)] Okazuję bilet i wsiadam! (Stan: 4/20, Rowery: 3/5) 
[21:25:55] [Pasażer 42 (Rower)] Okazuję bilet i wsiadam! (Stan: 5/20, Rowery: 4/5) 
[21:25:55] [Pasażer 12 (Zwykły)] Okazuję bilet i wsiadam! (Stan: 6/20, Rowery: 4/5)
[21:25:55] [Pasażer 47 (Rower)] Okazuję bilet i wsiadam! (Stan: 7/20, Rowery: 5/5)
[21:25:55] [Pasażer 4 (Zwykły)] Okazuję bilet i wsiadam! (Stan: 8/20, Rowery: 5/5)
...
[21:25:55] [Pasażer 19 (Zwykły)] Okazuję bilet i wsiadam! (Stan: 18/20, Rowery: 5/5)
[21:25:55] [Pasażer 15 (Zwykły)] Okazuję bilet i wsiadam! (Stan: 19/20, Rowery: 5/5) 
[21:25:55] [Pasażer 46 (Zwykły)] Okazuję bilet i wsiadam! (Stan: 20/20, Rowery: 5/5)
...
[21:26:00] [Autobus 1] Czas postoju minął.
[21:26:00] [Autobus 1] ODJAZD z 20 pasażerami (5 rowerów).
[21:26:08] [Autobus 1] WRÓCIŁ Z TRASY po 8 s.
[21:26:08] [Autobus 1] Podstawiłem się. CZEKAM NA PASAŻERÓW (Czas: 5s)!
...
[21:26:08] [Pasażer 36 (Zwykły)] Okazuję bilet i wsiadam! (Stan: 1/20, Rowery: 0/5) 
[21:26:08] [Pasażer 24 (Zwykły)] Okazuję bilet i wsiadam! (Stan: 2/20, Rowery: 0/5) 
[21:26:08] [Pasażer 18 (Zwykły)] Okazuję bilet i wsiadam! (Stan: 3/20, Rowery: 0/5) 
...
```

### Test C: Obsługa priorytetów (VIP)
* **Scenariusz:** W kolejce czekają pasażerowie Zwykli. Pojawia się VIP.
* **Dane wejściowe:** P=20, R=5, N=1, T_POSTOJ=5 (test ze zwiększoną szansą na VIP-a)
* **Przebieg:**
1. Omijanie kolejki (Kasa): VIP wysyła komunikat na dedykowany kanał `KANAL_KASA_VIP`. Kasa błyskawicznie potwierdza obsługę, dzięki czemu VIP natychmiast trafia na przystanek.
2. Gromadzenie na peronie: Na przystanku znajdują się już inni pasażerowie VIP oraz pasażerowie z rowerami.
3. Selektywne wybudzanie: Każdy z tych pasażerów "dzwoni" dzwonkiem `SEM_KTOS_CZEKA`. Autobus po każdym dzwonku skanuje pamięć współdzieloną.
4. Bezwzględny priorytet: Mimo obecności rowerzysty, pętla decyzyjna autobusu wykonuje serię otwarć semafora `SEM_KOLEJKA_VIP`. W efekcie pasażerowie VIP wsiadają jeden po drugim.
5. Obsługa reszty: Dopiero gdy `liczba_vip_oczekujacych` wynosi 0, autobus otwiera drzwi dla rowerzysty, co widać w ostatniej linii logów.
* **Rezultat:** ✅ Pozytywny. VIP wchodzi do autobusu natychmiast, z pominięciem kolejki.

Przykładowe fragmenty logów:
```text
...
[23:09:58] [Pasażer 84 (VIP)] Mam karnet, omijam kolejkę do kasy. (PID: 1256612)
[23:09:58] [KASA] VIP (PID: 1256612) - Obsługa poza kolejnością.
[23:09:58] [Pasażer 84 (VIP)] Przychodzę na przystanek.
...
[23:09:58] [Pasażer 57 (VIP)] Okazuję bilet i wsiadam! (Stan: 1/20, Rowery: 0/5)
[23:09:58] [Pasażer 54 (VIP)] Okazuję bilet i wsiadam! (Stan: 2/20, Rowery: 0/5)
[23:09:58] [Pasażer 91 (VIP)] Okazuję bilet i wsiadam! (Stan: 3/20, Rowery: 0/5)
[23:09:58] [Pasażer 84 (VIP)] Okazuję bilet i wsiadam! (Stan: 4/20, Rowery: 0/5) 
[23:09:58] [Pasażer 11 (Rower)] Okazuję bilet i wsiadam! (Stan: 5/20, Rowery: 1/5)
...
```

### Test D: Zależność Dziecko-Opiekun
* **Scenariusz:** Weryfikacja atomowego wejścia pary Opiekun + Dziecko.
* **Dane wejściowe:** P=21, R=5, N=1, T_POSTOJ=5
* **Przebieg:**
1. Proces Opiekuna tworzy wątek Dziecka `pthread_create`, który zasypia na zmiennej warunkowej.
2. Autobus sprawdza warunek wolne >= 2. Jeśli dostępne jest tylko 1 miejsce, para nie zostaje wpuszczona (brak rozdzielenia).
3. Wsiadanie: Opiekun inkrementuje licznik pasażerów o 2 w jednej sekcji krytycznej.
* **Rezultat:** ✅ Pozytywny. Dziecko podróżuje jako pasywny wątek wewnątrz procesu Opiekuna, poprawnie rezerwując zasoby autobusu.

Przykładowe fragmenty logów:
```text
[00:18:23] [Opiekun 67] Idę z dzieckiem (wątek utworzony).
[00:18:23] [Opiekun 69] Idę z dzieckiem (wątek utworzony).
[00:18:23] [Opiekun 68] Idę z dzieckiem (wątek utworzony).
[00:18:23] [Opiekun 70] Idę z dzieckiem (wątek utworzony).
...
[00:18:23] [Pasażer 65 (Opiekun)] Idę do kasy (PID: 1267828).
[00:18:23] [Pasażer 63 (Opiekun)] Idę do kasy (PID: 1267826).
[00:18:23] [KASA] Obsługuję opiekuna (PID: 1267824) i dziecko (TID: 140206211913408)
[00:18:23] [Pasażer 61 (Opiekun)] Przychodzę na przystanek.
[00:18:23] [KASA] Obsługuję opiekuna (PID: 1267822) i dziecko (TID: 140569384646336)
[00:18:23] [Pasażer 59 (Opiekun)] Przychodzę na przystanek.
...
[00:18:23] [Pasażer 9 (Opiekun)] Okazuję bilet i wsiadam! (Stan: 12/21, Rowery: 0/5)
[00:18:23] [Opiekun 9] Wprowadzam dziecko do autobusu.
[00:18:23] [Pasażer 5 (Opiekun)] Okazuję bilet i wsiadam! (Stan: 14/21, Rowery: 0/5)
[00:18:23] [Opiekun 5] Wprowadzam dziecko do autobusu.
[00:18:23] [Pasażer 10 (Opiekun)] Okazuję bilet i wsiadam! (Stan: 16/21, Rowery: 0/5)
[00:18:23] [Opiekun 10] Wprowadzam dziecko do autobusu.
[00:18:23] [Pasażer 12 (Opiekun)] Okazuję bilet i wsiadam! (Stan: 18/21, Rowery: 0/5)
[00:18:23] [Opiekun 12] Wprowadzam dziecko do autobusu.
[00:18:23] [Pasażer 39 (Opiekun)] Okazuję bilet i wsiadam! (Stan: 20/21, Rowery: 0/5)
[00:18:23] [Opiekun 39] Wprowadzam dziecko do autobusu.
```

### Test E: Interwencja Dyspozytora (Sygnał)
* **Scenariusz:** Użytkownik naciska klawisz `1` podczas załadunku.
* **Dane wejściowe:** P=21, R=5, N=1, T_POSTOJ=5
* **Przebieg:**
1. Dyspozytor: Odczytuje komendę i wysyła `SIGUSR1` do procesu nadrzędnego.
2. Main: Handler ustawia `flaga_odjazd = 1`. Proces główny identyfikuje `pid_obecnego_autobusu` i przekazuje sygnał `SIGUSR1` bezpośrednio do procesu autobusu na peronie.
3. Autobus: Sygnał przerywa blokujące oczekiwanie na semaforze.
4. Reakcja: Pętla załadunku zostaje przerwana przez warunek `if (wymuszony_odjazd) break;`.
* **Rezultat:** ✅ Pozytywny. Autobus odjechał natychmiast, nie czekając na pełny załadunek ani upływ czasu.

Przykładowe fragmenty logów:
```text
[DYSPOZYTOR ZEWN.] Otrzymano SIGUSR1 -> Rozkaz odjazdu!
[00:35:57] Wysyłam sygnał do autobusu PID 1270447
[00:35:57] [Autobus 1] Otrzymano nakaz natychmiastowego odjazdu!
[00:35:57] [Autobus 1] ODJAZD z 4 pasażerami (0 rowerów).
```

---
## Linki do kluczowych fragmentów

### a. Tworzenie i obsługa plików
Wykorzystano bibliotekę standardową (nakładka na syscall open/write) do logowania zdarzeń.
   
https://github.com/Bar-t-e-k/Symulacja_autobusu_podmiejskiego/blob/2c46a2e24341c8337f216a7229a4ffc224fc4dc0/logs.c#L24-L37

### b. Tworzenie procesów
Podstawa architektury. Proces główny tworzy procesy potomne, które zmieniają swój obraz pamięci (exec).
 
Kasjer: 
https://github.com/Bar-t-e-k/Symulacja_autobusu_podmiejskiego/blob/50dbfbbdcceaa187836ebf3f4435f6461eb8facf/main.c#L127-L142
Autobusy: 
https://github.com/Bar-t-e-k/Symulacja_autobusu_podmiejskiego/blob/50dbfbbdcceaa187836ebf3f4435f6461eb8facf/main.c#L145-L163
Pasażerowie: 
https://github.com/Bar-t-e-k/Symulacja_autobusu_podmiejskiego/blob/50dbfbbdcceaa187836ebf3f4435f6461eb8facf/main.c#L166-L216

### c. Obsługa sygnałów
Reakcja na interwencję Dyspozytora oraz bezpieczne zamykanie.

Dyspozytor: 
https://github.com/Bar-t-e-k/Symulacja_autobusu_podmiejskiego/blob/50dbfbbdcceaa187836ebf3f4435f6461eb8facf/main.c#L219-L257
Ctrl + C: 
https://github.com/Bar-t-e-k/Symulacja_autobusu_podmiejskiego/blob/50dbfbbdcceaa187836ebf3f4435f6461eb8facf/main.c#L262-L265
Sygnał 1: 
https://github.com/Bar-t-e-k/Symulacja_autobusu_podmiejskiego/blob/50dbfbbdcceaa187836ebf3f4435f6461eb8facf/main.c#L271-L282
Sygnał 2: 
https://github.com/Bar-t-e-k/Symulacja_autobusu_podmiejskiego/blob/50dbfbbdcceaa187836ebf3f4435f6461eb8facf/main.c#L284-L302

### e. Synchronizacja procesów
Wykorzystano semafory do ochrony zasobów i blokowania wejścia do drzwi.

https://github.com/Bar-t-e-k/Symulacja_autobusu_podmiejskiego/blob/54a08ec16af602a515bf1a322da68d768b53f861/ipc_utils.c#L61-L124

### g. Segmenty pamięci dzielonej
Współdzielenie stanu dworca między procesami.

https://github.com/Bar-t-e-k/Symulacja_autobusu_podmiejskiego/blob/54a08ec16af602a515bf1a322da68d768b53f861/ipc_utils.c#L14-L57

### h. Kolejki komunikatów
Komunikacja między Kasjerem a Pasażerami

https://github.com/Bar-t-e-k/Symulacja_autobusu_podmiejskiego/blob/54a08ec16af602a515bf1a322da68d768b53f861/ipc_utils.c#L128-L174

### i. Konfiguracja i walidacja
Dane wczytywane z pliku config.txt.

https://github.com/Bar-t-e-k/Symulacja_autobusu_podmiejskiego/blob/2c46a2e24341c8337f216a7229a4ffc224fc4dc0/config.c#L8-L75

### j. Wykorzystanie wątków
Dziecko jest tworzone jako wątek wewnątrz opiekuna.

https://github.com/Bar-t-e-k/Symulacja_autobusu_podmiejskiego/blob/54a08ec16af602a515bf1a322da68d768b53f861/exe_passenger.c#L24-L38

### k. Funkcje aktorów

Kasjer: 
https://github.com/Bar-t-e-k/Symulacja_autobusu_podmiejskiego/blob/54a08ec16af602a515bf1a322da68d768b53f861/exe_cashier.c#L14-L39
Autobus: 
https://github.com/Bar-t-e-k/Symulacja_autobusu_podmiejskiego/blob/50dbfbbdcceaa187836ebf3f4435f6461eb8facf/exe_bus.c#L22-L137
Pasażer: 
https://github.com/Bar-t-e-k/Symulacja_autobusu_podmiejskiego/blob/54a08ec16af602a515bf1a322da68d768b53f861/exe_passenger.c#L41-L212

### l. Obsługa błędów i logi
Wykorzystano własne funkcje do zapisywania logów i obsługi błędów

https://github.com/Bar-t-e-k/Symulacja_autobusu_podmiejskiego/blob/86c3ae1d4f3778efd698ad36cdce002232e9db5e/logs.c#L11-L45

---
**Autor:** Bartłomiej Zięcina
