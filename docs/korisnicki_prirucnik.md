# Korisnički priručnik

## BalloonBreath

### IoT digitalni spirometar na ESP32 platformi

---

# 1. Uvod

BalloonBreath je edukacijski sustav koji simulira rad medicinskog poticajnog spirometra (Incentive Spirometer).

Sustav koristi:

- ESP32 mikrokontroler
- OLED SSD1306 zaslon
- potenciometar kao simulator protoka zraka

Cilj korisnika je održavati simulirani protok zraka unutar zadanog medicinskog raspona tijekom pet sekundi.

---

# 2. Potrebna oprema

Za korištenje sustava potrebno je:

| Komponenta | Količina |
|------------|------------|
| ESP32 DevKit V4 | 1 |
| OLED SSD1306 128x64 | 1 |
| Potenciometar | 1 |
| USB kabel | 1 |

---

# 3. Pokretanje sustava

## Korak 1

Spojiti ESP32 s računalom.

---

## Korak 2

Pokrenuti projekt u [Wokwi simulatoru](https://wokwi.com/projects/465562606015886337) ili prenijeti program na fizički ESP32 uređaj.

---

## Korak 3

Nakon pokretanja na OLED zaslonu prikazuje se početni ekran.

Prikaz:

```text
BalloonBreath
R: 0
F: 0
READY
```

---

# 4. Korisničko sučelje

OLED zaslon prikazuje nekoliko informacija.

---

## 4.1 Naziv projekta

```text
BalloonBreath
```

Prikazuje naziv sustava.

---

## 4.2 Record

```text
R: 12345
```

Prikazuje najveći ostvareni volumen spremljen u NVS memoriji.

---

## 4.3 Flow

```text
F: 980
```

Prikazuje trenutni protok zraka izražen u ml/s.

---

## 4.4 Balloon

Balon predstavlja trenutnu jačinu udaha.

Pravila:

- slab protok → balon se nalazi nisko
- optimalan protok → balon se nalazi u sredini
- prejak protok → balon odlazi previsoko

---

## 4.5 Coach Indicator

Coach Indicator nalazi se na desnoj strani OLED zaslona.

Njegova svrha je:

- prikaz stabilnosti disanja
- simulacija medicinskog indikatora
- pružanje biofeedback informacije

Ako se pokazivač nalazi unutar ciljne zone:

disanje je dovoljno stabilno.

---

## 4.6 Volume

```text
V: 8450
```

Prikazuje ukupni volumen udaha.

---

## 4.7 Status

Moguće poruke:

| Poruka | Značenje |
|----------|----------|
| READY | Sustav čeka početak |
| H:2.4 | Odbrojavanje |
| SUCCESS | Uspješna vježba |
| FAST! | Prevelik protok |

---

# 5. Pravila igre

BalloonBreath koristi medicinska pravila spirometra.

---

## Uvjeti uspjeha

Korisnik mora:

- ostvariti protok veći od 900 ml/s
- ne prijeći 1200 ml/s
- održavati stabilnost veću od 80
- održavati uvjete 5 sekundi

---

## Uvjeti neuspjeha

Ako protok prijeđe:

```text
1200 ml/s
```

vježba se poništava.

Prikazuje se poruka:

```text
FAST!
```

Nakon kratkog vremena sustav se resetira.

---

# 6. Tijek korištenja

## Korak 1

Okretati potenciometar.

---

## Korak 2

Balon se počinje podizati.

---

## Korak 3

Pokušati zadržati balon u sigurnoj zoni.

---

## Korak 4

Coach Indicator mora ostati u ciljnom području.

---

## Korak 5

Održavati stanje pet sekundi.

---

## Korak 6

Ako je vježba uspješna:

prikazuje se poruka:

```text
SUCCESS
```

---

# 7. Primjeri korištenja

## Primjer 1 – Uspješan pokušaj

Flow:

```text
1000 ml/s
```

Stability:

```text
90
```

Vrijeme:

```text
5 s
```

Rezultat:

```text
SUCCESS
```

---

## Primjer 2 – Presnažan udah

Flow:

```text
1300 ml/s
```

Rezultat:

```text
FAST!
```

---

## Primjer 3 – Nestabilno disanje

Flow:

```text
950 ml/s
```

Stability:

```text
55
```

Rezultat:

timer se resetira.

---

# 8. Spremanje rezultata

Projekt koristi NVS memoriju.

Sprema se:

- najveći volumen

Prednost:

podaci ostaju spremljeni i nakon gašenja uređaja.

---

# 9. Serial Monitor

Sustav ispisuje dijagnostičke informacije.

Primjer:

```text
===== BALLOONBREATH =====

FLOW = 934 ml/s
FLOW = 945 ml/s
FLOW = 960 ml/s

[START]

FLOW = 1002 ml/s

[SUCCESS]

[NVS] Novi rekord: 8423
```

---

# 10. Rješavanje problema

## OLED je prazan

Provjeriti:

- SDA → GPIO21
- SCL → GPIO22
- I2C adresu 0x3C

---

## Potenciometar ne reagira

Provjeriti:

- SIG → GPIO34
- VCC → 3.3V
- GND → GND

---

## Rekord se ne sprema

Provjeriti:

- Preferences biblioteku
- ESP32 platformu

---

# 11. Sigurno korištenje

Projekt je edukacijska simulacija.

Nije namijenjen:

- medicinskoj dijagnostici
- kliničkoj uporabi
- terapijskim odlukama

Svi prikazani podaci služe isključivo za potrebe nastave i demonstracije rada ugrađenih sustava.

---

# 12. Zaključak

BalloonBreath omogućuje jednostavno i intuitivno izvođenje simulirane respiratorne vježbe korištenjem ESP32 platforme.

Kombinacijom OLED grafike, Coach Indicatora i pohrane rezultata sustav uspješno demonstrira osnovne principe rada digitalnog spirometra.
