# BalloonBreath

BalloonBreath je sustav koji simulira rad spirometra pomoću potenciometra te proces vježbi disanja pretvara u interaktivnu igru inspiriranu balonom na vrući zrak. Snaga puhanja simulira se potenciometrom, a ovisno o intenzitetu i stabilnosti disanja mijenja se prikaz na OLED zaslonu.

Cilj igre je održavati balon unutar sigurne zone protoka zraka između 900 ml/s i 1200 ml/s tijekom 5 sekundi. Ako je protok prenizak, balon pada prenisko, a ako je previsok balon izlazi iz sigurne zone i pokušaj nije uspješan.

Sustav na zaslonu prikazuje:
- trenutni protok zraka
- stabilnost disanja
- ukupni volumen udaha
- trajanje održavanja u ciljanoj zoni
- status uspješnosti pokušaja

Projekt koristi:
- ESP32
- OLED zaslon
- potenciometar za simulaciju disanja

Funkcionalnost uređaja temelji se na obradi analognih podataka, filtriranju signala i vizualnom prikazu rezultata u stvarnom vremenu.

## Autori
- Petra Štirjan
- Patrik Ostrunić

## Kolegij
Razvoj ugradbenih sustava
