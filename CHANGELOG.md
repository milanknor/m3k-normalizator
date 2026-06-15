# Changelog — M3K Normalizator

Všechny podstatné změny pluginu. Formát vychází z [Keep a Changelog](https://keepachangelog.com/),
verzování je [semantické](https://semver.org/lang/cs/): **MAJOR.MINOR.PATCH**

- **MAJOR** – velká/nekompatibilní změna (např. změna chování parametrů, jiný zvuk)
- **MINOR** – nová funkce při zachování zpětné kompatibility
- **PATCH** – oprava chyby nebo drobné doladění

> Po každé úpravě: přidej řádek nahoru pod „Nezveřejněno", a až to vydáš,
> přejmenuj na novou verzi + zvedni `project(... VERSION x.y.z)` v `CMakeLists.txt`.

## [Nezveřejněno]

## [0.1.10] – 2026-06-16
### Opraveno
- „Najíždění" v režimech Momentary a Short: řez dříve čekal na platné Momentary
  měření (~400 ms), takže hlasitý materiál hrál ~0,5 s naplno. Nyní se během warm-upu
  řeže podle okamžité hlasitosti bloku (~10 ms) → ztlumení do ~30–50 ms. (Boost
  zůstává hradlovaný, Integrated reaguje okamžitě dle persistované hodnoty.)

## [0.1.9] – 2026-06-16
### Opraveno
- „Najíždění" po spuštění v režimu Integrated: warm-up zbytečně čekal 400 ms, takže
  skladba ~0,5 s hrála naplno a pak teprve sjela. Integrovaná hodnota přežívá přes
  pauzu (ticho je z ní vyhradlované), takže je platná hned — zisk teď dosedne na
  správnou úroveň do ~30 ms místo 0,5 s.

## [0.1.8] – 2026-06-16
### Opraveno
- Náraz v režimu Custom s krátkým oknem: tiché okno se naplnilo okamžitě a změřilo
  tichý náběh skladby → boost +19 dB v prvním bloku. Nyní se zesílení povolí až po
  400 ms souvislého signálu ve všech režimech (ztlumení zůstává rychlé).

## [0.1.7] – 2026-06-16
### Opraveno
- Náraz po spuštění u HLASITÉ skladby: warm-up dříve držel jednotku (0 dB) po celou
  délku okna (až 3 s u Short), takže hlasitá skladba hrála naplno, než se začala řezat.
  Nyní se během warm-upu povolí okamžité ztlumení podle Momentary (~400 ms), jen boost
  čeká na naplnění okna. (Logování zatím ponecháno pro ověření.)

## [0.1.6] – 2026-06-16
### Přidáno (dočasné — diagnostika)
- Zápis do logu `Dokumenty\M3K_Normalizator_log.txt` — pro ladění nárazů hlasitosti.
  Loguje přechody ticho/signál, hlasitý výstup a stav (úrovně, ref, zisk, limiter).
  Po vyřešení bude logování odstraněno.

## [0.1.5] – 2026-06-16
### Opraveno
- Náraz hlasitosti po pauze: plugin teď nezesiluje, dokud se měřicí okno nenaplní
  skutečným signálem (po tichu hraje na původní úrovni, pak se plynule dorovná).
  Řeší zbylé případy, kdy po pauze a opětovném spuštění občas vystřelilo nahlas

## [0.1.4] – 2026-06-16
### Opraveno
- VU metr: barva výplně teď odpovídá skutečné úrovni (zelená dole, žlutá k 0 dB,
  červená až nahoře) — dřív gradient ukazoval všechny barvy bez ohledu na úroveň

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
