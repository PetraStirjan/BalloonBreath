# BalloonBreath

## Digitalni poticajni spirometar: ESP32 + OLED + potenciometar + Wi-Fi

BalloonBreath je edukacijska simulacija poticajnog spirometra. Umjesto pravog
senzora tlaka koristi se potenciometar. OLED sučelje prilagođeno je pacijentima:
prikazuje jedan veliki balon, sigurnu zonu i kratke motivirajuće poruke.

Detaljni tehnički podaci ne opterećuju mali OLED zaslon. Oni se arhiviraju u NVS
memoriju i šalju stručnoj osobi putem Wi-Fi/HTTPS JSON izvještaja.

---

# Pregled zahtjeva

## A. Akvizicija i obrada signala

- Offset: sredina potenciometra, oko ADC 2048, predstavlja mirovanje.
- Smjerovi: desno od sredine je UDAH, lijevo od sredine je IZDAH.
- EMA filtriranje signala pomoću `EMA_ALPHA = 0.35`.
- Numerička integracija volumena `V = integral Q dt`.

## B. HMI

- Jedan glavni balon s tri medicinska praga: 600, 900 i 1200 ml/s.
- Sigurna zona jasno je označena između 900 i 1200 ml/s.
- Coach Indicator prikazuje ciljnu zonu stabilnosti 80-100.
- Inercija balona simulira masu i otpor zraka.
- Pacijentske poruke na zaslonu: READY, STRONGER, ALMOST, HOLD, GREAT,
  TOO FAST, STEADY, TRY AGAIN i TIME.

## C. IoT povezivost

- Wi-Fi spajanje preko `WiFi.h`.
- HTTPS/HTTP POST slanje rezultata nakon ciklusa vježbe.
- NVS povijest: najbolji volumen i najbolji vršni protok.
- OLED podsjetnik ako uspješna vježba nije obavljena u predviđenom vremenu.
- JSON komunikacijski protokol za preglednu arhivu rezultata.

## Logika uspješne vježbe

- Kvantiteta: balon mora biti u zoni koja odgovara protoku od najmanje 900 ml/s.
- Kvaliteta: stabilnost mora biti u ciljnoj zoni, najmanje 80.
- Trajanje: uvjet mora trajati neprekidno 5 sekundi.
- Kazna: ako protok dosegne 1200 ml/s, pokušaj se poništava.

## Obvezna implementacija

- Neblokirajući rad bez `delay()` u glavnoj petlji.
- Modularna organizacija koda po funkcijama.
- Filtriranje ulaznog signala.
- OLED prikaz stanja vježbe.
- Detekcija uspjeha i neuspjeha.
- Dokumentacija spajanja i opisa rješenja.

## Inženjerske odluke

- Konstanta filtriranja: `EMA_ALPHA = 0.35`.
- Model inercije: balon ima poziciju i brzinu, uz prigušeni pomak prema
  ciljnoj poziciji.
- Upravljanje pogreškama: nagli trzaj potenciometra detektira se preko
  `MAX_FLOW_STEP = 350` i označava vježbu kao nestabilnu.
- Komunikacijski protokol: JSON payload za arhivu namijenjenu liječniku ili
  stručnoj osobi.

---

# Hardver

| Komponenta | Spajanje |
| ---------- | -------- |
| Potenciometar SIG | GPIO34 |
| Potenciometar VCC | 3V3 |
| Potenciometar GND | GND |
| OLED SDA | GPIO21 |
| OLED SCL | GPIO22 |
| OLED VCC | 3V3 |
| OLED GND | GND |

---

# OLED prikaz za pacijenta

Mali OLED ne prikazuje sve brojeve odjednom. Glavni ekran prikazuje:

- naziv projekta
- jedan veliki balon
- tri praga protoka: 600, 900 i 1200 ml/s
- sigurnu zonu 900-1200 ml/s
- Coach Indicator za stabilnost
- kratku poruku pacijentu
- rezultat volumena nakon uspješne vježbe

Poruke su namjerno jednostavne:

| Poruka | Značenje |
| ------ | -------- |
| READY | Sustav čeka udah |
| STRONGER | Udah je preslab |
| ALMOST | Korisnik je blizu sigurne zone |
| HOLD | Korisnik je u dobroj zoni |
| GREAT | Vježba je uspješna |
| TOO FAST | Udah je presnažan |
| STEADY | Protok je nestabilan |
| TRY AGAIN | Pokušaj je prekinut |
| TIME | Vrijeme je za novu vježbu |

Tehnički podaci kao volumen, vršni protok i prosječna stabilnost šalju se serveru
i ispisuju u Serial Monitoru.

---

# IoT, Wi-Fi i HTTPS

ESP32 se u Wokwiju spaja na:

```cpp
#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_PASSWORD ""
```

Rezultat se nakon završenog ciklusa šalje na:

```cpp
#define SERVER_URL "https://httpbin.org/post"
```

Kod podržava `https://` i `http://` URL-ove. Za HTTPS se koristi
`WiFiClientSecure`. U demo načinu koristi se `setInsecure()` kako Wokwi ne bi
trebao lokalni certifikat. U stvarnom medicinskom sustavu treba koristiti pravi
server i root CA certifikat umjesto `setInsecure()`.

Primjer JSON poruke:

```json
{
  "device_id": "BalloonBreath-ESP32",
  "result": "SUCCESS",
  "volume_ml": 5388.0,
  "peak_flow_ml_s": 1066.0,
  "avg_stability": 94.2,
  "best_volume_ml": 10981.0,
  "best_peak_flow_ml_s": 1180.0,
  "duration_ms": 7200,
  "penalty": false
}
```

Slanje se dokazuje na dva načina:

1. U Serial Monitoru vidi se HTTP kod, JSON i kratak odgovor servera.
2. Za vizualni dokaz može se koristiti servis kao Webhook.site: kopira se vlastiti
   URL u `SERVER_URL`, pokrene se vježba i zahtjev se vidi u web pregledniku.

`httpbin.org` je dobar za test jer vraća poslani JSON u odgovoru, ali ne daje
trajni web pregled arhive. Za predaju ili demonstraciju praktičniji je
Webhook.site ili vlastiti mali server.

---

# Testiranje

1. Potenciometar u sredini: prikaz READY, protok 0, volumen ne raste.
2. Desno iznad 600: vježba prelazi u RUNNING.
3. Stabilno 900-1200 kroz 5 s: prikazuje GREAT i šalje JSON.
4. Preko 1200: prikazuje TOO FAST i šalje JSON s `penalty: true`.
5. Lijevo od sredine: prikazuje BREATHE, volumen ne raste.
6. Nagli trzaj: prikazuje STEADY i resetira timer.
7. Nakon uspjeha resetirati ESP32: NVS rekord ostaje spremljen.
8. Pričekati 60 s bez uspjeha: prikazuje se TIME.

---

# Wokwi

Projekt je dostupan na Wokwi poveznici:

https://wokwi.com/projects/466203868077330433

Za Wokwi su bitne datoteke:

- `wokwi/sketch.ino`
- `wokwi/diagram.json`
- `wokwi/libraries.txt`

Datoteke `src/sketch.ino` i `wokwi/sketch.ino` moraju ostati sinkronizirane.
