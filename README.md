# BalloonBreath

## RUS Projekt – Digitalni IoT Spirometar

BalloonBreath je interaktivna simulacija medicinskog poticajnog spirometra razvijena na ESP32 platformi.

Projekt koristi potenciometar kao emulator protoka zraka te OLED zaslon za prikaz respiratorne vježbe kroz igru balona na vrući zrak.

Cilj korisnika je održati balon unutar sigurne zone disanja tijekom 5 sekundi, uz dovoljno stabilan protok zraka.

---

# Opis projekta

Tradicionalni medicinski poticajni spirometar koristi tri kuglice kalibrirane na:

* 600 ml/s
* 900 ml/s
* 1200 ml/s

Pacijent mora održavati stabilan protok zraka kako bi pravilno izvodio respiratorne vježbe.

U projektu BalloonBreath isti medicinski princip prikazan je kroz simulaciju leta balona na vrući zrak.

### Pravila igre

* protok mora biti veći od 900 ml/s
* protok mora biti manji od 1200 ml/s
* disanje mora biti stabilno
* uvjet mora trajati neprekidno 5 sekundi

Ako protok prijeđe 1200 ml/s:

* aktivira se kazna
* pokušaj se poništava
* korisnik mora započeti novu vježbu

---

# Korišteni hardver

* ESP32 DevKit C V4
* SSD1306 OLED zaslon (128x64)
* Potenciometar
* USB napajanje

---

# Korištene biblioteke

```cpp
Wire
Adafruit_GFX
Adafruit_SSD1306
Preferences
```

---

# Funkcionalnosti

## Obrada signala

* ADC očitavanje potenciometra
* EMA filtriranje signala
* mapiranje na protok zraka

## Coach Indicator

Praćenje stabilnosti disanja korištenjem biofeedback indikatora.

## Numerička integracija

Izračun ukupnog volumena:

V = ∫Qdt

## NVS memorija

Spremanje najboljeg ostvarenog volumena u trajnu memoriju ESP32 uređaja.

## OLED grafičko sučelje

Prikaz:

* balona
* protoka zraka
* stabilnosti
* volumena
* vremena u ciljnoj zoni
* statusa vježbe

---

# Stanja sustava

Projekt koristi konačni automat stanja (Finite State Machine).

## IDLE

Početno stanje.

Čeka početak vježbe.

## RUNNING

Vježba je aktivna.

Prati se:

* protok
* stabilnost
* vrijeme

## SUCCESS

Uspješno održavanje uvjeta tijekom 5 sekundi.

Rezultat se sprema ako je ostvaren novi rekord.

## PENALTY

Aktivira se kada protok prijeđe:

```text
1200 ml/s
```

Pokušaj se poništava.

---

# Spajanje komponenti

| OLED | ESP32  |
| ---- | ------ |
| SDA  | GPIO21 |
| SCL  | GPIO22 |
| VCC  | 3V3    |
| GND  | GND    |

| Potenciometar | ESP32  |
| ------------- | ------ |
| SIG           | GPIO34 |
| VCC           | 3V3    |
| GND           | GND    |

---

# Wokwi simulacija

Projekt je razvijen i testiran korištenjem Wokwi simulatora.

Potrebne komponente:

* ESP32 DevKit C V4
* SSD1306 OLED
* Potenciometar

---

# Struktura repozitorija

```text
BalloonBreath/
│
├── README.md
├── Doxyfile
│
├── docs/
│   ├── izvjestaj.md
│   └── korisnicki_prirucnik.md
│
├── rezultat/
│   ├── balloonbreath.png
│   └── serial_output.txt
│
├── src/
│   └── sketch.ino
│
├── wokwi/
│   ├── diagram.json
│   ├── libraries.txt
│   ├── sketch.ino
│   └── wokwi-project.txt
│
└── wiki/
    ├── 1.-Opis-projektnog-zadatka.md
    ├── 2.-Arhitektura-sustava.md
    ├── 3.-Implementacija.md
    ├── 4.-Testiranje-i-rezultati.md
    └── 5.-Zakljucak.md
```

---

# Rezultati

Projekt uspješno ostvaruje:

* simulaciju digitalnog spirometra
* praćenje stabilnosti disanja
* numerički izračun volumena
* trajnu pohranu rezultata
* grafički prikaz u stvarnom vremenu

Svi funkcionalni zahtjevi projektnog zadatka uspješno su implementirani.

---

# Licenca

Projekt je razvijen isključivo u obrazovne svrhe za potrebe kolegija Računalni ugrađeni sustavi.
