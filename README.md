# FMOD FM Synthesizer DSP Plugin (`fmod_fm_synth_dsp`)

A high-performance, real-time 2-operator **Frequency Modulation (FM) Synthesizer DSP Plugin** for **FMOD Core Engine** and **FMOD Studio 2.x**.

Designed for game audio, procedural sound design, and interactive audio systems, this plugin generates rich dynamic timbres ranging from pure sine tones and warm electric pianos to metallic bells, organs, basses, and harsh sci-fi textures.

---

## 🎛️ Mathematical Model & Synthesis Architecture

The plugin implements classic Chowning 2-operator phase modulation synthesis:

$$s(t) = V \cdot \sin\Big(2\pi f_c t + I \cdot \sin(2\pi f_m t)\Big)$$

where:
- $f_c$ = **Carrier Frequency** (frequenza fondamentale della portante in Hz)
- $R$ = **Modulator Ratio** ($f_m / f_c$, rapporto armonico C:M)
- $f_m = f_c \times R$ = **Modulator Frequency** (frequenza dell'operatore modulante)
- $I$ = **Modulation Index** ($\beta$, profondità di modulazione)
- $V$ = **Volume** (ampiezza di uscita lineare)

### Real-Time Audio Guarantees
- **Lock-Free Parameter Synchronization**: Parameters are exchanged between the main/UI thread and the real-time audio thread using `std::atomic<float>` with `std::memory_order_relaxed`.
- **Zero Heap Allocations in Audio Thread**: No calls to `malloc`, `free`, `new`, or `delete` in the DSP process callback.
- **Continuous Synthesis on Idle Buses**: Implements `SquareWaveDSP_ShouldIProcess` / `FMSynthDSP_ShouldIProcess` returning `FMOD_OK` unconditionally, ensuring audio continues to render even when inserted on an idle mixer bus or stopped track.
- **Multi-Channel Routing**: Correctly maps stereo and surround speaker channels and masks during `FMOD_DSP_PROCESS_QUERY`.

---

## 📊 Parameters

| Index | Parameter Name | Range | Default | Scale | Description |
| :---: | :--- | :---: | :---: | :---: | :--- |
| `0` | **Carrier Freq** | 20.0 Hz – 20,000.0 Hz | 440.0 Hz | Logarithmic | Frequenza fondamentale dell'onda portante |
| `1` | **Mod Ratio** | 0.10 – 16.00 | 1.00 | Lineare | Rapporto di frequenza $f_m / f_c$ (C:M) |
| `2` | **Mod Index** | 0.00 – 20.00 | 2.00 | Lineare | Indice di modulazione (brillantezza e bande laterali) |
| `3` | **Volume** | 0.00 – 1.00 | 0.20 | Lineare / dB | Guadagno lineare di uscita |

### Guida Sonora ai Rapporti di Frequenza (Mod Ratio)
- **$R = 1.00$**: Spettro armonico fondamentale caldo (organo, ottoni morbidi).
- **$R = 2.00$**: Onde quadre/dente di sega ricche di armoniche (bassi acidi, synth lead).
- **$R = 3.00, 4.00, 5.00$**: Timbre metallici chiari, campane tubolari.
- **$R = 1.414, 2.718$ (Inarmonico)**: Gongs, piatti, campane tibetane, rumori metallici e suoni fantascientifici.
- **$I = 0.00$**: Genera una pura onda sinusoidale a frequenza $f_c$. Aumentando $I$, compaiono le bande laterali di Bessel che arricchiscono il timbro.

---

## 📁 Struttura della Repository

```
fmod_fm_synth_dsp/
├── CMakeLists.txt                      # Build system multipiattaforma (C++17)
├── include/
│   ├── FMSynthDSP.h                    # Dichiarazione classe DSP ed export C
│   └── fmod/                           # Header ufficiali FMOD Core
│       ├── fmod.h
│       ├── fmod.hpp
│       ├── fmod_common.h
│       └── fmod_dsp.h
├── src/
│   └── FMSynthDSP.cpp                  # Implementazione sintesi FM & callbacks
├── fmod_studio/
│   ├── FMSynthDSP.plugin.xml           # Descrittore metadata FMOD Studio
│   └── FMSynthDSP.plugin.js            # UI Deck widget interattivo per FMOD Studio
├── .github/workflows/
│   └── build-and-release.yml           # CI/CD multipiattaforma per GitHub Releases
└── README.md
```

---

## 🛠️ Compilazione da Riga di Comando (CLI)

### Prerequisiti
- **CMake >= 3.20**
- Compilatore C++17 supportato:
  - **macOS**: Apple Clang (Xcode Command Line Tools)
  - **Windows**: Microsoft Visual Studio 2019/2022 (MSVC)
  - **Linux**: GCC >= 9 o Clang >= 10

### macOS (Universal Binary: Apple Silicon arm64 + Intel x86_64)
```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --config Release
codesign -s - -f build/fmod_fm_synth_dsp.dylib
```
*Output binario:* `build/fmod_fm_synth_dsp.dylib`

### Windows (MSVC x64)
```cmd
cmake -B build -S . -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```
*Output binario:* `build/Release/fmod_fm_synth_dsp.dll`

### Linux (x86_64)
```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
*Output binario:* `build/fmod_fm_synth_dsp.so`

---

## 📦 Installazione in FMOD Studio

Per utilizzare il plugin all'interno dell'interfaccia grafica di **FMOD Studio**:

### 1. Posizione dei File

Copia i 3 file del plugin:
1. `fmod_fm_synth_dsp.dylib` (o `.dll` su Windows / `.so` su Linux)
2. `fmod_studio/FMSynthDSP.plugin.xml`
3. `fmod_studio/FMSynthDSP.plugin.js`

In una delle due posizioni seguenti:

#### Opzione A: A livello di Progetto FMOD (Consigliato)
Copia i file nella cartella `Plugins/` del tuo progetto:
```
<TuoProgettoFMOD>/Plugins/
├── fmod_fm_synth_dsp.dylib
├── FMSynthDSP.plugin.xml
└── FMSynthDSP.plugin.js
```

#### Opzione B: A livello Globale per tutti i progetti
- **macOS**: `~/Library/Application Support/FMOD Studio/Plugins/`
- **Windows**: `%LOCALAPPDATA%\FMOD Studio\Plugins\`
- **Linux**: `~/.local/share/FMOD Studio/Plugins/`

### 2. Utilizzo nel Mixer di FMOD Studio
1. Avvia **FMOD Studio** e apri il tuo progetto.
2. Apri la vista **Mixer** (<kbd>F3</kbd> o <kbd>Cmd</kbd>+<kbd>2</kbd>).
3. Seleziona una traccia o il **Master Bus**.
4. Nella deck degli effetti in basso, fai clic destro e scegli:
   **Add Effect** ➔ **FM Synthesizer**.
5. Vedrai comparire i 4 controlli interattivi:
   - Manopola **Carrier Freq** (20 Hz - 20 kHz)
   - Manopola **Mod Ratio** (0.1x - 16x)
   - Manopola **Mod Index** (0.0 - 20.0)
   - Fader **Volume** (0.0 - 1.0)
6. Il sintetizzatore inizierà a suonare in tempo reale!

---

## 💻 Integrazione Run-Time C++ (FMOD Core)

Di seguito un esempio minimale in C++17 che illustra come caricare la libreria dinamica del plugin a runtime, istanziare il DSP e modularne i parametri in un'applicazione o motore di gioco:

```cpp
#include "fmod.hpp"
#include "fmod_errors.h"
#include <iostream>
#include <thread>
#include <chrono>

void CheckError(FMOD_RESULT result, const char* functionName)
{
    if (result != FMOD_OK)
    {
        std::cerr << "Errore in " << functionName << ": " 
                  << FMOD_ErrorString(result) << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

int main()
{
    FMOD::System* system = nullptr;
    CheckError(FMOD::System_Create(&system), "System_Create");
    CheckError(system->init(32, FMOD_INIT_NORMAL, nullptr), "System::init");

    // 1. Carica il plugin dinamico a runtime
#if defined(_WIN32)
    const char* pluginPath = "fmod_fm_synth_dsp.dll";
#elif defined(__APPLE__)
    const char* pluginPath = "fmod_fm_synth_dsp.dylib";
#else
    const char* pluginPath = "fmod_fm_synth_dsp.so";
#endif

    unsigned int pluginHandle = 0;
    CheckError(system->loadPlugin(pluginPath, &pluginHandle, 0), "System::loadPlugin");

    // 2. Istanzia il DSP dal plugin caricato
    FMOD::DSP* fmSynthDSP = nullptr;
    CheckError(system->createDSPByPlugin(pluginHandle, &fmSynthDSP), "System::createDSPByPlugin");

    // 3. Avvia la riproduzione del DSP sul Master Channel Group
    FMOD::Channel* channel = nullptr;
    CheckError(system->playDSP(fmSynthDSP, nullptr, false, &channel), "System::playDSP");

    std::cout << "Riproduzione FM Synthesizer avviata a 440 Hz..." << std::endl;

    // 4. Modula i parametri a runtime (Carrier = 440Hz, Ratio = 2.0x, Index = 3.5)
    fmSynthDSP->setParameterFloat(0, 440.0f); // Carrier Freq
    fmSynthDSP->setParameterFloat(1, 2.0f);   // Mod Ratio (1:2)
    fmSynthDSP->setParameterFloat(2, 3.5f);   // Mod Index
    fmSynthDSP->setParameterFloat(3, 0.25f);  // Volume

    // Suona per 2 secondi
    for (int i = 0; i < 20; ++i)
    {
        system->update();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Passa a un timbro metallico inarmonico (Ratio = 1.414, Index = 6.0)
    std::cout << "Modulazione timbro metallico inarmonico..." << std::endl;
    fmSynthDSP->setParameterFloat(1, 1.414f);
    fmSynthDSP->setParameterFloat(2, 6.0f);

    for (int i = 0; i < 30; ++i)
    {
        system->update();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Pulizia risorse
    if (channel) channel->stop();
    if (fmSynthDSP) fmSynthDSP->release();
    system->unloadPlugin(pluginHandle);
    system->close();
    system->release();

    return 0;
}
```

---

## 📄 Licenza

Rilasciato sotto licenza MIT.
