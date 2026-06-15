# Changelog — M3K Normalizator

Všechny podstatné změny pluginu. Formát vychází z [Keep a Changelog](https://keepachangelog.com/),
verzování je [semantické](https://semver.org/lang/cs/): **MAJOR.MINOR.PATCH**

- **MAJOR** – velká/nekompatibilní změna (např. změna chování parametrů, jiný zvuk)
- **MINOR** – nová funkce při zachování zpětné kompatibility
- **PATCH** – oprava chyby nebo drobné doladění

> Po každé úpravě: přidej řádek nahoru pod „Nezveřejněno", a až to vydáš,
> přejmenuj na novou verzi + zvedni `project(... VERSION x.y.z)` v `CMakeLists.txt`.

## [Nezveřejněno]

## [0.1.3] – 2026-06-15
### Přidáno
- Kliknutí na logo v levém horním rohu otevře malé okno „o aplikaci" s logem a číslem verze
  (nad logem se zobrazí ručička)

## [0.1.2] – 2026-06-15
### Opraveno
- VU metry po zastavení přehrávání zůstávaly viset na poslední hodnotě —
  nyní se při tichu / přerušení signálu plynule stáhnou na nulu (heartbeat z audio vlákna)

## [1.0.0] – 2026-06-15
První verzovaný stav. Plugin obsahuje:

### Měření
- LUFS metr: Momentary (400 ms), Short (3 s), Integrated (od resetu) — K-vážení (EBU R128 / ITU-R BS.1770-4)
- C-vážené varianty modů (IEC 61672) + dva Custom mody (A/K i C) s volitelným oknem 10–10000 ms
- Správné sčítání kanálů (stereo) místo průměrování → přesný target
- LRA (Loudness Range, EBU Tech 3342) zvlášť pro vstup a výstup
- Stereo VU metry (peak, L/R) na vstupu i výstupu
- Rolující 60s graf s křivkami M/S/I a dB stupnicí

### Normalizace
- Auto-normalizace na cílový LUFS (-60 až 0), boost i cut
- Hradlování ticha — zisk se po pauze nevytočí a nepraští při startu skladby
- Ticho se nezapočítává do Integrated (absolutní brána -70 LUFS)
- Asymetrické vyhlazení: rychle dolů (~30 ms), nahoru dle Speed (10–4000 ms)
- Boost omezen na +24 dB jako pojistka
- Bezpečnostní výstupní strop -1 dBFS (peak-limiter, instant attack)

### UI
- Logo M3K + název v hlavičce, zobrazení verze
- Kulaté ovladače s hodnotou uvnitř, kulatá políčka LRA
- Tlačítka modů (2 řady: A / C), Normalize + Reset v hlavičce
- Kompaktní okno 544×470
