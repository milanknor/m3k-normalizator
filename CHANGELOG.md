# Changelog — M3K Normalizator

Všechny podstatné změny pluginu. Formát vychází z [Keep a Changelog](https://keepachangelog.com/),
verzování je [semantické](https://semver.org/lang/cs/): **MAJOR.MINOR.PATCH**

- **MAJOR** – velká/nekompatibilní změna (např. změna chování parametrů, jiný zvuk)
- **MINOR** – nová funkce při zachování zpětné kompatibility
- **PATCH** – oprava chyby nebo drobné doladění

> Po každé úpravě: přidej řádek nahoru pod „Nezveřejněno", a až to vydáš,
> přejmenuj na novou verzi + zvedni `project(... VERSION x.y.z)` v `CMakeLists.txt`.

## [Nezveřejněno]

## [0.9.x] – 2026-06-20
### Přidáno
- **Dvě rychlosti adaptace**: ovladač **DOWN** (ztišení hlasitého náporu) a **UP**
  (nenápadný náběh tichého základu) — místo jednoho Speed. Program-dependent UP release
  (malé korekce se zpomalí, potlačí mikro-pumpování). Tovární presety nastavují obě.
- **Relativní brána (EBU R128 −10 LU)** — Integrated se počítá dvoustupňovým hradlováním
  (histogram 400ms momentary bloků po 100 ms). Měření ověřeno proti pyloudnorm na 0,011 LU.
- **Číselné zadávání hodnot** — dvojklik na knob = napsat hodnotu, Ctrl+klik = výchozí.
- **Graf: přepínač LOUDNESS / FADER** — Fader pohled ukazuje per-režim průběh zisku
  (aktivní plnou čarou, ostatní přerušovaně), počítaný se stejným hradlováním jako reálný
  fader. Přidána **Custom (C) křivka** a políčko. Klik na M/S/I/C/FADER skryje/zobrazí křivku.
- **Nápověda** (tlačítko „?" vpravo nahoře) — rolovací návod k použití.
### Změněno
- **GAIN přejmenován na FADER**, barva sjednocena s gain křivkou. Tlačítka režimů obarvena
  podle křivek (M zelená, S modrá, I oranžová, C teal). Tmavé téma popup menu. Okno širší (624 px).
### Opraveno
- **True-peak limiter** — odstraněno zkreslení při vysoké hlasitosti (kytary). Limiter měl
  chybné časování (zisk řízen okamžitým vstupem, aplikován na zpožděný signál) → pouštěl
  špičky na 0 dBFS a tvrdě ořezával. Nově sliding-window minimum (peak-hold) přes lookahead
  + dvoustupňové vyhlazení + brickwall na stropu. THD 0,285 % → **0,022 %**, strop se drží.
  (Ověřeno simulací `verify_limiter.py`.)

## [0.8.0] – 2026-06-19
### Přidáno
- **Vstupní gain (trim)** — malý ovladač nad levým (IN) VU metrem, symetrický ke kontrolce
  CÍL vpravo. Rozsah −24 až +24 dB, dvojklik = 0 dB. Aplikuje se na vstup před měřením
  i zpracováním (s rampou proti lupancům) → ovlivní VU IN, LUFS i limiter. Pod ovladačem
  je hodnota (zelená = zesílení, oranžová = zeslabení).

## [0.7.0] – 2026-06-18
### Přidáno
- **Standalone systray** — okno se zavřením/minimalizací skryje do systray (ikona M);
  pravý klik = menu (Zobrazit/Skrýt, Nastavení zvuku, Ukončit). Funguje jako audio bridge
  (např. s VB-Audio Virtual Cable).
- **VU metry: číselné hodnoty** přímo v metrech (0/−6/−12/−24/−48 dB).
- **Integrovaný čas** v levém horním rohu grafu (počítá jen signál nad branou −70 LUFS).
### Změněno
- Klik na kterékoli LRA kolečko resetuje **obě** LRA; RESET I resetuje jen Integrated.
- Compliance LED reaguje podle aktivního módu (Momentary/Short/Custom/Integrated).
- Všech 6 koleček dole rovnoměrně rozloženo přes šířku.

## [0.5.5] – 2026-06-16
### Změněno
- Sekundární popisky zesvětleny: `dimCol` z `#555555` → `#888888` (lepší kontrast na tmavém pozadí).
- Označení M/S/I/GAIN/LIM ve value pruhu a IN/OUT na VU metrech přepsány na světlejší `txtCol` (`#CCCCCC`).

## [0.5.4] – 2026-06-16
### Změněno
- Klik na **kterékoli** LRA kolečko (IN nebo OUT) nyní vynuluje **obě** LRA měření najednou.
- Tlačítko **RESET I** resetuje výhradně Integrated měření (beze změny LRA).

## [0.5.3] – 2026-06-16
### Změněno
- Zapnuto **LTO** (link-time optimization) — VST3 zmenšeno z 8,2 na 7,1 MB a o kousek
  rychlejší kód. Export VST3 (moduleinfo.json) ověřen jako funkční.

## [0.5.2] – 2026-06-16
### Změněno
- Vendor/Company pluginu změněn na **Milan Knor** (i copyright v About okně).
### Přidáno
- **České tooltipy i nad indikátory** (M/S/I/GAIN/LIM pruh, VU metry, LRA, graf,
  kontrolka cíle, logo) — nájezdem myši.

## [0.5.1] – 2026-06-16
### Změněno
- Ovladač **Ceiling přejmenován na Limiter**.
- **Odstraněn BYPASS** přepínač.
- Compliance „výstup vs cíl" je nyní **barevná kontrolka (LED) nad pravým VU metrem**
  — zelená = v cíli (±0,5 LU), žlutá = blízko, červená = mimo, šedá = bez dat.
### Přidáno
- **České tooltipy** při nájezdu myší na ovladače a tlačítka.

## [0.5.0] – 2026-06-16
### Přidáno
- **True-peak limiter** — limiter nově běží ve 4× oversamplingu, takže hlídá i
  mezivzorkové (true-peak) špičky → soulad s dBTP normami i před převodem do MP3/AAC.
  (Přidává malou latenci, hostitel ji kompenzuje.)
- **Gain-reduction měřič** (buňka „LIM" v horním pruhu) — kolik dB limiter právě ubírá.
- **Compliance indikátor** — vpravo ve status řádku „VÝSTUP vs CÍL": zelená „✓ V CÍLI"
  když výstupní Integrated sedí na cíl (±0,5 LU), jinak odchylka v LU (žlutá/červená).
- **BYPASS** přepínač (status řádek vlevo, i jako host-bypass) — A/B porovnání originálu.
- Horní pruh rozšířen na 5 buněk (M, S, I, GAIN, LIM), okno o něco vyšší.

## [0.4.2] – 2026-06-16
### Změněno
- Všechna kolečka dole (4 knoby + 2 LRA políčka) mají nyní stejný průměr (64 px).

## [0.4.1] – 2026-06-16
### Změněno
- Tovární předvolby nově nastavují i **Speed** (streaming 1000 ms, broadcast 1500 ms,
  podcast 1200 ms, klub/DJ 500/300 ms) — předvolba je teď kompletní a předvídatelná.

## [0.4.0] – 2026-06-16
### Přidáno
- Tovární předvolby podle standardů (menu PRESET): Streaming (Spotify, Apple Music,
  YouTube, Amazon, Tidal, Deezer + Spotify Loud), Broadcast/TV (EBU R128, ATSC A/85,
  TR-B32), Podcast, Klub/DJ. Každá nastaví Target LUFS, Ceiling, mód Integrated a
  zapne normalizaci.
- Nový ovladač **CEILING** (strop limiteru, −6 až 0 dBFS) — řiditelný ručně i přes
  předvolby (např. −1 dBFS pro streaming).
- Uložit/Načíst preset přesunuto do menu PRESET (místo dvou tlačítek).
- Okno o něco vyšší kvůli 4. ovladači.

## [0.3.0] – 2026-06-16
### Přidáno
- Presety: tlačítka SAVE / LOAD v hlavičce ukládají a načítají nastavení do souboru
  (`*.m3kpreset` ve složce Dokumenty\M3K Normalizator Presets). Načtení se aplikuje
  jen na danou instanci — presety ani instance se nemíchají.
- Verze přesunuta do levého dolního rohu (uvolnění místa pro SAVE/LOAD).

## [0.2.9] – 2026-06-16
### Přidáno
- Kliknutím na kolečko LRA IN nebo LRA OUT se vynuluje příslušné LRA měření
  (nad kolečky se zobrazí ručička).

## [0.2.8] – 2026-06-16
### Změněno
- Strop limiteru zvednut z −1 dBFS na **−0,3 dBFS** — maximální hlasitost na hraně.

## [0.2.7] – 2026-06-16
### Změněno
- Odstraněno diagnostické logování — vyhlazení limiteru (lookahead) ověřeno jako
  funkční. Plugin už nezapisuje na disk.

## [0.2.6] – 2026-06-16
### Změněno
- Vyhlazení limiteru při vysokém targetu: nahrazen okamžitý peak-limiter **lookahead
  limiterem** (~1,5 ms). Zisk teď plynule sjede před špičkou místo skokového slámování
  po samplech → konec „roztřeseného" zvuku při silném zesílení. (Přidává 1,5 ms latenci,
  hostitel ji kompenzuje.) Logování zatím ponecháno.

## [0.2.5] – 2026-06-16
### Přidáno (dočasné — diagnostika)
- Znovu zapnuto diagnostické logování (`Dokumenty\M3K_Normalizator_log.txt`) kvůli
  ladění další nesrovnalosti.

## [0.2.4] – 2026-06-16
### Změněno
- Odstraněno diagnostické logování — pumpování i nápory ověřeny jako vyřešené
  (gain stabilní, limiter jen mírně a legitimně ořezává špičky u hlasitých cílů).
  Plugin už nezapisuje na disk.

## [0.2.3] – 2026-06-16
### Opraveno (nalezeno v logu, ověřeno sim_pump2.py)
- Pumpování i v režimu Momentary (a obecně): momentary klesá v tichých pasážích →
  boost honil propad až na +18 dB → limiter slámoval. Boost-cap dle momentary (0.2.2)
  tu nepomohl (ref = momentary). Nyní je boost omezen **Integrated** hlasitostí (stabilní
  celková úroveň stopy, +3 dB) → boost dotahuje stopu na target, ne každý tichý moment.
  Ověřeno: peak boost +12,4 → +7,0 dB, tichá stopa pořád dojede na +12 dB.

## [0.2.2] – 2026-06-16
### Opraveno (nalezeno v logu, ověřeno simulací sim_pump.py)
- Pumpování v režimu Custom s krátkým oknem + rychlým Speed: měřicí okno chytalo tiché
  mezery mezi beaty (ref padal až na -36 LUFS) → boost vyletěl na +24 dB → limiter
  slámoval (lim 0,50, stovky zásahů). Nyní je boost omezen stabilní 400ms Momentary
  hlasitostí (+3 dB) → gain se drží klidně, žádné pumpování. (Ověřeno: +23,7 → +7,6 dB.)
  Netýká se cutu ani normálního boostu tiché stopy.

## [0.2.1] – 2026-06-16
### Přidáno (dočasné — diagnostika)
- Vráceno diagnostické logování (`Dokumenty\M3K_Normalizator_log.txt`) kvůli ladění
  dalších nesrovnalostí.

## [0.2.0] – 2026-06-16
### Změněno
- Odstraněno diagnostické logování — chování po pauze potvrzeno jako stabilní.
  Plugin už nezapisuje na disk. (Simulační skript sim_normalizer.py zůstává v repu.)

## [0.1.11] – 2026-06-16
### Opraveno (ověřeno simulací všech 8 režimů)
- Kompletní přepracování chování zisku po pauze/spuštění (`sim_normalizer.py` testuje
  scénáře řez/boost/cross přes všechny režimy — žádný nepřesáhne +3 dB nad target):
  - během ticha se **drží řez** (boost se uvolní k jednotce) → spuštění stejné stopy
    bez přestřelení
  - **rychlejší sjezd zisku** (10 ms, warm-up řez 5 ms)
  - **reset Integrated po tichu** (>0,2 s) → konec náporu při změně z tiché na hlasitou
    stopu (dříve až +15 dB)
  - Integrated nově jako okénkový režim (po pauze krátký warm-up místo okamžité reakce
    na zastaralou hodnotu)

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
