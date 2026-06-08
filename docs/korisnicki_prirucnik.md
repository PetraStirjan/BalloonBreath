# Korisnički priručnik

## BalloonBreath

BalloonBreath je simulacija poticajnog spirometra na ESP32 platformi. OLED prikaz
prilagođen je pacijentima: prikazuje jedan balon, sigurnu zonu i kratke poruke.

---

# 1. Pokretanje u Wokwiju

Wokwi projekt dostupan je na poveznici:

https://wokwi.com/projects/466203868077330433

1. Otvoriti Wokwi projekt.
2. U `sketch.ino` zalijepiti sadržaj iz `wokwi/sketch.ino`.
3. Provjeriti jesu li OLED i potenciometar spojeni kao u `diagram.json`.
4. Kliknuti Run.
5. Otvoriti Serial Monitor za Wi-Fi, HTTPS i JSON poruke.

Za Wokwi Wi-Fi koristi:

```cpp
WIFI_SSID = "Wokwi-GUEST"
WIFI_PASSWORD = ""
```

---

# 2. Spajanje

| Komponenta | ESP32 |
| ---------- | ----- |
| Potenciometar SIG | GPIO34 |
| Potenciometar VCC | 3V3 |
| Potenciometar GND | GND |
| OLED SDA | GPIO21 |
| OLED SCL | GPIO22 |
| OLED VCC | 3V3 |
| OLED GND | GND |

---

# 3. Korištenje potenciometra

Potenciometar se koristi kao emulator daha:

- sredina: mirovanje
- desno od sredine: udah
- lijevo od sredine: izdah

Mala mrtva zona oko sredine sprječava lažne male protoke.

---

# 4. OLED prikaz

OLED prikazuje:

- naziv BalloonBreath
- Wi-Fi indikator u gornjem desnom kutu
- jedan veliki balon
- oznake 600, 900 i 1200 ml/s
- sigurnu zonu 900-1200 ml/s
- Coach Indicator desno
- kratku poruku pacijentu
- rezultat volumena nakon uspjeha

Poruke:

| Poruka | Značenje |
| ------ | -------- |
| READY | Sustav čeka udah |
| STRONGER | Udah je preslab |
| ALMOST | Korisnik je blizu sigurne zone |
| HOLD | Korisnik je u sigurnoj zoni |
| GREAT | Vježba je uspješna |
| TOO FAST | Protok je dosegao 1200 ml/s |
| STEADY | Nagli trzaj ili nestabilan protok |
| TRY AGAIN | Pokušaj je prekinut |
| TIME | Podsjetnik za vježbu |

---

# 5. Pravila vježbe

Vježba počinje kada udah prijeđe 600 ml/s.

Za uspjeh treba održati:

- udah
- protok od najmanje 900 ml/s
- protok manji od 1200 ml/s
- stabilnost najmanje 80
- rad bez naglog trzaja
- uvjete neprekidno 5 sekundi

Ako protok dosegne 1200 ml/s, pokušaj se poništava i prikazuje se `TOO FAST`.

---

# 6. Wi-Fi, HTTPS i slanje podataka

Nakon završetka ciklusa ESP32 šalje JSON poruku serveru. Zadani testni server je:

```cpp
https://httpbin.org/post
```

U kodu se to mijenja ovdje:

```cpp
#define SERVER_URL "https://httpbin.org/post"
```

Ako URL počinje s `https://`, program koristi `WiFiClientSecure`. U demonstraciji
se koristi `setInsecure()` kako bi Wokwi mogao slati HTTPS bez dodavanja certifikata.
Za stvarnu medicinsku primjenu treba koristiti pravi server i root CA certifikat.

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

---

# 7. Kako dokazati da slanje radi

## Opcija 1: Serial Monitor

Nakon uspjeha ili kazne u Serial Monitoru treba se vidjeti:

```text
[REPORT] Queued SUCCESS
[HTTP] POST https://httpbin.org/post code=200
{... JSON ...}
[HTTP] Response preview: ...
```

`code=200` znači da je server primio zahtjev.

## Opcija 2: Webhook.site

Za vidljiv dokaz u pregledniku:

1. Otvoriti Webhook.site.
2. Kopirati jedinstveni URL koji stranica generira.
3. U kodu zamijeniti `SERVER_URL` tim URL-om.
4. Pokrenuti vježbu u Wokwiju.
5. Nakon slanja, POST zahtjev i JSON vide se na Webhook.site stranici.

`httpbin.org` je dobar za test jer vraća odgovor u Serial Monitor, ali ne čuva
javnu povijest zahtjeva. Webhook.site je bolji za demonstraciju profesorima.

---

# 8. NVS povijest

ESP32 pamti:

- najbolji volumen
- najbolji vršni protok

Podaci ostaju spremljeni nakon resetiranja jer se koristi NVS `Preferences`.

---

# 9. Testovi

1. Potenciometar u sredini: status READY, protok 0.
2. Pomaknuti desno iznad 600: vježba počinje.
3. Držati 900-1200 ml/s kroz 5 sekundi: GREAT.
4. Otići preko 1200 ml/s: TOO FAST.
5. Pomaknuti lijevo: sustav traži udah, volumen ne raste.
6. Naglo trznuti: STEADY.
7. Pričekati 60 sekundi bez uspjeha: TIME.
8. Nakon SUCCESS provjeriti Serial Monitor ili Webhook.site za POST.

---

# 10. Napomena

Projekt je edukacijska simulacija i nije namijenjen medicinskoj dijagnostici ili
kliničkoj uporabi.
