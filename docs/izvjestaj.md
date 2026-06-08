# Izvještaj projekta BalloonBreath

## Projekt

Digitalni poticajni spirometar na ESP32 platformi

Wokwi projekt:

https://wokwi.com/projects/466203868077330433

## Autori

- Petra Štirjan
- Patrik Ostrunić

---

# 1. Sažetak

BalloonBreath je edukacijska simulacija poticajnog spirometra izvedena na ESP32
platformi. Sustav koristi potenciometar kao emulator protoka daha, OLED zaslon
kao korisničko sučelje te Wi-Fi vezu za slanje rezultata vježbe poslužitelju.

U klasičnom mehaničkom poticajnom spirometru pacijent udiše zrak kako bi podigao
tri kuglice kalibrirane na protoke od 600, 900 i 1200 ml/s. U ovom projektu
zadržani su isti medicinski pragovi, ali je prikaz prilagođen pacijentima kroz
jednostavnije i motivirajuće sučelje s balonom. Balon predstavlja trenutni protok
udaha, dok sigurna zona označava raspon u kojem pacijent treba zadržati dah.

Projekt obrađuje analogni signal potenciometra, filtrira ga eksponencijalnim
kliznim prosjekom, određuje smjer disanja, računa volumen numeričkom
integracijom, provjerava stabilnost udaha, vodi logiku vježbe u stvarnom vremenu
i šalje sažetak rezultata u JSON formatu.

---

# 2. Uvod

Poticajni spirometri koriste se u respiratornoj rehabilitaciji, posebno nakon
kirurških zahvata, dugotrajnog ležanja ili stanja koja smanjuju plućnu funkciju.
Njihova je svrha potaknuti pacijenta na kontroliran i dovoljno dubok udah, uz
izbjegavanje prenaglog ili nestabilnog disanja.

Zbog ograničenja laboratorijske opreme u ovom projektu ne koristi se stvarni
senzor tlaka ili protoka zraka. Umjesto toga, potenciometar služi kao emulator
protoka daha. Takav pristup omogućuje razvoj i testiranje ugradbenog programa
bez pneumatske instalacije, a zadržava glavne zahtjeve zadatka: obradu signala,
prikaz protoka, procjenu stabilnosti i provjeru uspješnosti vježbe.

Posebna pažnja posvećena je komentaru profesora da je ciljana publika pacijent,
a ne stručna osoba. Zato OLED prikaz nije opterećen svim tehničkim podacima.
Pacijentu se prikazuje samo ono što mu je potrebno tijekom izvođenja vježbe:
balon, sigurna zona, indikator stabilnosti i kratka jasna poruka. Detaljniji
podaci i dalje se prikupljaju u pozadini te se mogu koristiti za kasniju analizu.

---

# 3. Cilj projekta

Cilj projekta bio je izraditi ugrađeni sustav koji simulira rad poticajnog
spirometra i provjerava kvalitetu vježbe disanja prema zadanim medicinskim
uvjetima.

Sustav mora razlikovati mirovanje, udah i izdah, prikazati tri razine protoka od
600, 900 i 1200 ml/s, voditi vježbu bez blokirajućih kašnjenja i prepoznati
uspješnu vježbu samo ako pacijent stabilno održava protok u sigurnoj zoni od
900 do 1200 ml/s tijekom pet sekundi.

Ako protok dosegne ili prijeđe 1200 ml/s, pokušaj se poništava jer to predstavlja
prenagli udah. Time se simulira medicinski nepoželjan način izvođenja vježbe,
kod kojeg previsok protok može značiti lošiju kontrolu disanja.

---

# 4. Hardverska konfiguracija

Projekt je razvijen za ESP32 DevKit V4 u Wokwi simulacijskom okruženju. Sustav se
sastoji od tri glavne komponente:

| Komponenta | Uloga u sustavu |
| ---------- | --------------- |
| ESP32 DevKit V4 | Glavni mikrokontroler, obrada signala, logika vježbe i Wi-Fi komunikacija |
| Potenciometar | Emulator protoka daha |
| OLED SSD1306 | Prikaz stanja vježbe i povratna informacija pacijentu |

Spajanje je izvedeno na sljedeći način:

| Signal | ESP32 pin |
| ------ | --------- |
| Potenciometar SIG | GPIO34 |
| Potenciometar VCC | 3V3 |
| Potenciometar GND | GND |
| OLED SDA | GPIO21 |
| OLED SCL | GPIO22 |
| OLED VCC | 3V3 |
| OLED GND | GND |

Potenciometar je spojen na analogni ulaz GPIO34 jer ESP32 na tom pinu može
očitavati 12-bitnu ADC vrijednost u rasponu od 0 do 4095.

---

# 5. Obrada ulaznog signala

Srednji položaj potenciometra predstavlja mirovanje. Budući da je ADC 12-bitni,
sredina raspona nalazi se oko vrijednosti 2048. Oko te vrijednosti definirana je
mrtva zona kako male oscilacije potenciometra ne bi pokretale vježbu.

U programu su definirane konstante:

```cpp
#define ADC_CENTER 2048
#define DEADZONE 80
```

Ako je očitana vrijednost unutar mrtve zone, sustav je u stanju mirovanja. Ako je
vrijednost veća od sredine, smjer se tumači kao udah. Ako je vrijednost manja od
sredine, smjer se tumači kao izdah.

Smjer disanja u programu je zapisan pomoću enumeracije:

```cpp
enum BreathDirection {
  REST,
  INHALE,
  EXHALE
};
```

Takvo mapiranje omogućuje da se volumen i uspješnost vježbe računaju samo tijekom
udaha. Izdah se prikazuje kao zasebno stanje, ali ne pridonosi volumenu niti
pokreće mjerenje uspjeha.

---

# 6. Filtriranje i izračun volumena

Analogni signal potenciometra može biti nestabilan, posebno pri naglim pomacima.
Zbog toga se koristi eksponencijalni klizni prosjek, odnosno EMA filtar:

```cpp
filteredFlow = EMA_ALPHA * rawFlow + (1.0 - EMA_ALPHA) * filteredFlow;
```

Odabrana vrijednost `EMA_ALPHA = 0.35` predstavlja kompromis između brzog odziva
i smirenog prikaza. Manja vrijednost dala bi stabilniji, ali sporiji prikaz, dok
bi veća vrijednost bila osjetljivija na trzaje.

Ukupni volumen računa se numeričkom integracijom protoka kroz vrijeme:

```cpp
volume += currentFlow * dt;
```

Budući da je protok izražen u ml/s, a `dt` u sekundama, rezultat integracije je
volumen u mililitrima. Volumen se povećava samo kada je vježba aktivna i kada je
smjer disanja udah.

---

# 7. Logika vježbe

Program je organiziran kao konačni automat stanja. Takav pristup omogućuje jasnu
podjelu ponašanja sustava i neblokirajući rad bez korištenja `delay()` funkcije.

Glavna stanja su:

| Stanje | Opis |
| ------ | ---- |
| IDLE | Sustav čeka početak udaha |
| RUNNING | Vježba je aktivna |
| SUCCESS | Vježba je uspješno završena |
| PENALTY | Pokušaj je poništen zbog prejakog udaha |
| ABORTED | Pokušaj je prekinut jer uvjeti nisu održani |

Vježba započinje kada korisnik napravi udah i protok prijeđe 600 ml/s. Tijekom
aktivne vježbe sustav stalno provjerava protok, stabilnost i trajanje uvjeta.

Uspjeh se priznaje samo ako su istovremeno zadovoljeni sljedeći uvjeti:

- smjer je udah
- protok je najmanje 900 ml/s
- protok je manji od 1200 ml/s
- stabilnost je najmanje 80 od 100
- nije detektiran nagli trzaj potenciometra
- uvjeti traju neprekidno pet sekundi

Ako protok dosegne 1200 ml/s, aktivira se stanje `PENALTY`. Tada se pokušaj
poništava i korisnik mora započeti novu vježbu.

---

# 8. Stabilnost i zaštita od pogrešaka

Stabilnost udaha računa se iz promjene filtriranog protoka između dva uzorka.
Ako se protok mijenja polako, stabilnost ostaje visoka. Ako se protok naglo
mijenja, stabilnost pada.

Pojednostavljeni izračun stabilnosti je:

```cpp
diff = fabs(currentFlow - prevFlow);
stability = constrain(100 - diff / 4, 0, 100);
```

Osim toga, uvedena je posebna zaštita od naglog trzanja potenciometra. Ako je
promjena sirovog protoka između dva uzorka veća od dopuštene granice, sustav to
tretira kao nestabilan pokušaj:

```cpp
jerkDetected = rawFlowStep > MAX_FLOW_STEP;
```

Na taj način se sprječava da korisnik naglim pomicanjem potenciometra umjetno
ostvari uvjete vježbe.

---

# 9. OLED sučelje za pacijenta

Prva verzija prikaza sadržavala je više tehničkih podataka, što je korisno za
razvoj, ali nije idealno za pacijenta. Konačno sučelje zato je pojednostavljeno.
Na zaslonu se prikazuje jedan balon, tri praga protoka, sigurna zona, Coach
Indicator i kratka poruka.

Balon zamjenjuje mehaničke kuglice, ali zadržava istu medicinsku logiku:

| Protok | Značenje u prikazu |
| ------ | ------------------ |
| 600 ml/s | početak korisnog udaha |
| 900 ml/s | donja granica sigurne zone |
| 1200 ml/s | gornja granica, nakon koje slijedi kazna |

Sigurna zona nalazi se između 900 i 1200 ml/s. Korisnik treba održavati balon u
toj zoni, dok Coach Indicator pokazuje koliko je protok stabilan.

Poruke na zaslonu namjerno su kratke i napisane na engleskom jeziku zbog
ograničene rezolucije OLED zaslona:

| Poruka | Značenje |
| ------ | -------- |
| READY | Sustav čeka početak |
| STRONGER | Udah je preslab |
| ALMOST | Korisnik je blizu sigurne zone |
| HOLD | Korisnik treba zadržati stabilan udah |
| GREAT | Vježba je uspješno završena |
| TOO FAST | Udah je presnažan |
| STEADY | Protok je nestabilan |
| TRY AGAIN | Pokušaj treba ponoviti |
| TIME | Vrijeme je za novu vježbu |

Tehnički podaci poput volumena, vršnog protoka i prosječne stabilnosti ne
zagušuju glavni prikaz, nego se šalju poslužitelju i spremaju u memoriju.

---

# 10. Inercija balona

Kako bi prikaz bio ugodniji i realističniji, balon se ne pomiče trenutno na novu
poziciju. Umjesto toga, koristi se jednostavan model inercije. Balon ima trenutnu
poziciju i brzinu, a prema ciljnoj poziciji pomiče se postupno.

Osnovna ideja modela je:

```cpp
force = (targetY - balloonY) * 34.0;
balloonVelocity += force * dt;
balloonVelocity *= 0.70;
balloonY += balloonVelocity * dt;
```

Time se simuliraju masa i prigušenje. Prikaz zato ne trza pri svakoj manjoj
promjeni signala, nego se ponaša prirodnije i pacijentu daje čitljiviju povratnu
informaciju.

---

# 11. IoT povezivost

ESP32 se spaja na Wi-Fi mrežu i nakon svakog završenog ciklusa šalje rezultat
vježbe poslužitelju. U Wokwi simulaciji koristi se mreža:

```cpp
#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_PASSWORD ""
```

Za demonstraciju slanja koristi se HTTP testni servis:

```cpp
#define SERVER_URL "https://httpbin.org/post"
```

Ako URL počinje s `https://`, program koristi `WiFiClientSecure`. U simulaciji se
koristi `setInsecure()` kako bi HTTPS komunikacija radila bez lokalno učitanog
certifikata. To je prihvatljivo za demonstraciju u Wokwiju, ali u stvarnoj
primjeni trebalo bi koristiti vlastiti poslužitelj i ispravan root CA certifikat.

Rezultat se šalje kao JSON poruka. Primjer sadržaja:

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

Slanje se može dokazati kroz Serial Monitor, gdje se ispisuje HTTP kod, poslani
JSON i dio odgovora poslužitelja. `httpbin.org` vraća poslani sadržaj u odgovoru,
ali ne čuva javnu povijest zahtjeva. Ako je potreban vizualan dokaz za
prezentaciju, umjesto `httpbin.org` može se privremeno koristiti Webhook.site jer
on prikazuje svaki zaprimljeni POST zahtjev u web sučelju.

---

# 12. Trajna memorija i podsjetnik

Za spremanje najboljeg rezultata koristi se ESP32 NVS memorija preko biblioteke
`Preferences`. Spremaju se najbolji volumen i najbolji vršni protok:

```cpp
bestVol
bestPeak
```

Vrijednosti se učitavaju pri pokretanju sustava i šalju u JSON izvještaju nakon
vježbe. Na taj način se rezultat ne gubi resetiranjem ESP32 mikrokontrolera.

Sustav također ima podsjetnik. Ako uspješna vježba nije obavljena unutar
predviđenog vremena, OLED u stanju mirovanja prikazuje poruku `TIME`. U
simulaciji je interval postavljen na 60 sekundi kako bi se funkcija mogla brzo
testirati.

---

# 13. Testiranje

Testiranje je provedeno u Wokwi simulaciji promjenom položaja potenciometra i
praćenjem OLED prikaza te Serial Monitora.

| Testni slučaj | Očekivano ponašanje |
| ------------- | ------------------- |
| Potenciometar u sredini | Sustav prikazuje READY, protok je 0, volumen ne raste |
| Lagani pomak udesno ispod 600 ml/s | Vježba se ne priznaje kao uspješna |
| Stabilan udah između 900 i 1200 ml/s kroz 5 s | Prikazuje se GREAT i šalje se izvještaj |
| Protok preko 1200 ml/s | Prikazuje se TOO FAST i pokušaj se poništava |
| Pomak lijevo od sredine | Sustav prepoznaje izdah, volumen se ne povećava |
| Nagli trzaj potenciometra | Prikazuje se STEADY i timer uspjeha se resetira |
| Reset ESP32 nakon uspjeha | Najbolji rezultat ostaje spremljen u NVS memoriji |
| Neaktivnost dulja od intervala podsjetnika | Prikazuje se TIME |

Testiranjem je potvrđeno da sustav razlikuje udah, izdah i mirovanje, da
primjenjuje medicinske pragove 600, 900 i 1200 ml/s te da uspjeh priznaje samo
ako su uvjeti održani neprekidno pet sekundi.

---

# 14. Ograničenja i moguća poboljšanja

Projekt je simulacijski, pa potenciometar ne može u potpunosti zamijeniti stvarni
senzor protoka ili diferencijalnog tlaka. U stvarnoj primjeni bilo bi potrebno
kalibrirati uređaj sa stvarnim senzorom i provjeriti točnost mjerenja na
medicinski prihvatljiv način.

OLED zaslon rezolucije 128 x 64 px ograničava količinu informacija koja se može
čitko prikazati. Zato je korisničko sučelje namjerno pojednostavljeno, a detaljni
podaci se šalju poslužitelju.

Moguća buduća poboljšanja su:

- korištenje stvarnog senzora protoka ili tlaka
- razvoj web aplikacije za liječnika
- prikaz povijesti vježbi kroz grafove
- korištenje vlastitog HTTPS poslužitelja s certifikatom
- dodavanje korisničkih profila za više pacijenata
- dulje testiranje s različitim parametrima filtriranja

---

# 15. Zaključak

BalloonBreath uspješno demonstrira kako se zahtjevi poticajnog spirometra mogu
prenijeti u ugrađeni sustav s ESP32 mikrokontrolerom. Sustav zadržava bitne
medicinske pragove od 600, 900 i 1200 ml/s, provjerava stabilnost udaha, računa
volumen, sprema najbolji rezultat i šalje podatke poslužitelju.

Kroz ovaj projekt naučili smo kako se jednostavan analogni ulaz može pretvoriti
u smislen pokazatelj ako se pravilno definiraju nulta točka, smjer signala,
filtriranje i vremenska logika. Također smo naučili da korisničko sučelje nije
samo estetski dodatak, nego važan dio funkcionalnosti, posebno kada je sustav
namijenjen pacijentima.

Najvažnije iskustvo bilo je povezivanje više područja u jednu cjelinu: obrada
analognih signala, neblokirajuće programiranje, modeliranje inercije, OLED
grafika, trajna memorija i Wi-Fi/HTTPS komunikacija. Projekt nam je pokazao da
ugradbeni sustav mora biti tehnički ispravan, ali i razumljiv korisniku koji ga
treba koristiti u stvarnoj vježbi.
