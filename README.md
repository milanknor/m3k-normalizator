# M3K Normalizator

Real-time LUFS loudness normalizer and metering plugin for Windows.  
Available as **VST3** (for DAWs) and **Standalone** desktop application.

**Author:** Milan Knor  
**License:** AGPL-3.0-or-later (free / open source)  
**Platform:** Windows 10/11 (x64)  
**Latest release:** see [Releases](../../releases)

---

## Features

- **8 measurement modes** — Momentary (400 ms), Short (3 s), Integrated, Custom window; each in K-weighting (EBU R128) and C-weighting (IEC 61672)
- **Auto-normalization** to target LUFS (−60 to 0) with separate **UP/DOWN** speeds (program-dependent release)
- **EBU R128 integrated** with two-stage gating (absolute −70 LUFS + relative −10 LU); verified against pyloudnorm to ~0.01 LU
- **Intermediate compressor** (glue/leveler) — loudness-neutral, with gain-reduction meter
- **True-peak limiter** — 4× oversampling, lookahead, configurable ceiling (dBTP)
- **LRA measurement** (EBU Tech 3342) for input and output separately
- **Graph views** — Loudness / Fader (per-mode gain preview) / **Spectrum** (FFT analyzer)
- **Stereo VU meters** with dB scale, 60-second rolling graph, compliance LED
- **Input gain trim**, **master output volume**
- **Factory presets** — Spotify, Apple Music, YouTube, EBU R128, ATSC A/85, Podcast, Club/DJ and more
- **Save/Load presets** (`.m3kpreset` files)
- **Standalone with system tray** — audio bridge, tray volume flyout, dark theme

## Standalone — virtual audio bridge

The standalone application works as a real-time audio bridge:

1. Install [VB-Audio Virtual Cable](https://vb-audio.com/Cable/) (free)
2. Set **CABLE Input** as the default Windows audio output
3. Launch **M3K Normalizator.exe** and go to *Audio Settings* (tray icon → right-click)
4. Set input: **CABLE Output**, output: your real speakers/headphones
5. Minimize to tray — the normalizer runs in the background

You can also select any other input device instead of VB-Cable.

## Download

See [Releases](../../releases) for the latest pre-built VST3 + Standalone ZIP.

## Building from source

Requirements: **Windows**, **Visual Studio 2019+**, **CMake 3.22+**, **JUCE 8** (place in `/JUCE` folder)

```bat
cmake -B build -S .
cmake --build build --config Release --parallel
```

Output:
- `build\M3KNormalizator_artefacts\Release\VST3\M3K Normalizator.vst3`
- `build\M3KNormalizator_artefacts\Release\Standalone\M3K Normalizator.exe`

## VST3 Installation

Copy `M3K Normalizator.vst3` to:
```
C:\Program Files\Common Files\VST3\
```
Then rescan plugins in your DAW.

## Loudness standards reference

| Platform | Target | Ceiling |
|---|---|---|
| Spotify | −14 LUFS | −1 dBTP |
| Apple Music | −16 LUFS | −1 dBTP |
| YouTube | −14 LUFS | −1 dBTP |
| EBU R128 (Broadcast) | −23 LUFS | −1 dBTP |
| ATSC A/85 (US TV) | −24 LUFS | −2 dBTP |
| Podcast | −16 LUFS | −1 dBTP |

## License

Copyright © 2026 Milan Knor.

This program is free software: you can redistribute it and/or modify it under the
terms of the **GNU Affero General Public License v3.0** (AGPL-3.0-or-later) as
published by the Free Software Foundation. See [LICENSE](LICENSE) for the full text.

It is distributed in the hope that it will be useful, but **WITHOUT ANY WARRANTY**;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

### Third-party

- **[JUCE](https://juce.com)** — audio/GUI framework, used under its open-source
  **AGPLv3** license. (Commercial use requires a JUCE licence.)
- **VST3 SDK** — © Steinberg Media Technologies, used under GPLv3 / the Steinberg VST3 licence.
  *VST* is a trademark of Steinberg Media Technologies GmbH.

Because this project links JUCE under AGPLv3, the **complete source code** of every
released binary is published in this repository.
