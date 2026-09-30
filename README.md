# FMOD Core DSP: Square Wave Generator (`fmod_square_dsp`)

Plugin DSP generatore di onde quadre mono/stereo in C++17 ad alte prestazioni per **FMOD Core Engine** e **FMOD Studio**. Progettato secondo i più rigorosi standard del real-time audio programming: thread-safety lock-free, zero allocazioni dinamiche nel render thread, interpolazione logaritmica della frequenza ed export nativo multi-piattaforma (Windows `.dll`, macOS `.dylib`, Linux `.so`).

---

## Indice
1. [Architettura Tecnica](#architettura-tecnica)
2. [Struttura del Repository](#struttura-del-repository)
3. [Prerequisiti di Sistema](#prerequisiti-di-sistema)
4. [Compilazione da Terminale (CLI)](#compilazione-da-terminale-cli)
   - [Windows (MSVC / Visual Studio)](#1-windows-msvc--visual-studio-20192022)
   - [macOS (Clang / Apple Silicon o Intel)](#2-macos-clang--apple-silicon-o-intel)
   - [Linux (GCC / Clang)](#3-linux-gcc-o-clang)
   - [Output Attesi](#output-dei-binari-compilati)
5. [Installazione e Configurazione in FMOD Studio](#installazione-e-configurazione-in-fmod-studio)
6. [Integrazione Runtime in C++ / Game Engine](#integrazione-runtime-in-c--game-engine)
7. [Specifiche dei Parametri](#specifiche-dei-parametri)

---

## Architettura Tecnica

- **Sintesi Real-Time Safe:** L'algoritmo sintetizza un'onda quadra bipolare pura $\in [-\text{Vol}, +\text{Vol}]$ mediante un accumulatore di fase normalizzato nell'intervallo $[0.0, 1.0)$.
- **Zero Allocazioni e Lock-Free:** Nessuna chiamata a `new`/`malloc`, chiamate di sistema o primitive bloccanti (`std::mutex`, `std::condition_variable`) all'interno del callback `SquareWaveDSP_Process`.
- **Sincronizzazione Atomica:** I parametri (`Frequency` e `Volume`) vengono scambiati tra thread principale (GUI FMOD Studio, thread logico di gioco) e thread audio a bassa latenza tramite `std::atomic<float>` con semantica `memory_order_relaxed`.
- **Mappatura Logaritmica:** La frequenza (20.0 Hz – 20,000.0 Hz) adotta una mappatura a tratti (`FMOD_DSP_PARAMETER_FLOAT_MAPPING_PIECEWISE_LINEAR`) per garantire una percezione musicale naturale e uniforme sull'escursione dei controlli UI.
- **Supporto Mono, Stereo e Multicanale:** Scrittura diretta nei buffer interleaved di output per tutti i canali configurati dalla pipeline FMOD.

---

## Struttura del Repository

```text
fmod_square_dsp/
├── CMakeLists.txt                      # Build system CMake multipiattaforma (C++17)
├── include/
│   └── SquareWaveDSP.h                 # Dichiarazione classe DSP, parametri e callback C API
├── src/
│   └── SquareWaveDSP.cpp               # Implementazione DSP, logica di sintesi e symbol export
├── fmod_studio/
│   └── SquareWaveDSP.plugin.xml        # Definizione metadati e deck UI per FMOD Studio
└── README.md                           # Guida completa di compilazione, uso e integrazione
```

---

## Prerequisiti di Sistema

Prima di procedere, assicurati di avere a disposizione:

1. **Compilatore C++17:**
   - **Windows:** Microsoft Visual C++ (MSVC) v142/v143 (Visual Studio 2019 o 2022 con workload "Desktop development with C++").
   - **macOS:** Apple Clang (Xcode o Xcode Command Line Tools: `xcode-select --install`).
   - **Linux:** GCC 9+ o Clang 10+.
2. **CMake:** Versione $\ge 3.20$ ([cmake.org](https://cmake.org/download/)).
3. **FMOD Engine / Core SDK:** Versione 2.0x, 2.1x, 2.2x o superiore scaricabile da [fmod.com/download](https://www.fmod.com/download).
   - È necessario il percorso assoluto alla cartella dell'API contenente la sottocartella `inc` (o `include`) e le relative librerie (`lib/`).

---

## Compilazione da Terminale (CLI)

Il progetto utilizza la variabile CMake `FMOD_SDK_DIR` per localizzare l'SDK di FMOD.

### 1. Windows (MSVC / Visual Studio 2019/2022)

Apri il **x64 Native Tools Command Prompt for VS** (o PowerShell) e posizionati nella cartella radice:

```cmd
:: Configurazione con generatore Visual Studio 17 2022
cmake -B build -S . -G "Visual Studio 17 2022" -A x64 ^
      -DFMOD_SDK_DIR="C:/Program Files (x86)/FMOD SoundSystem/FMOD Studio API Windows" ^
      -DCMAKE_BUILD_TYPE=Release

:: Compilazione della DLL Release
cmake --build build --config Release
```

*In alternativa con Ninja:*
```cmd
cmake -B build -S . -G "Ninja" -DCMAKE_BUILD_TYPE=Release ^
      -DFMOD_SDK_DIR="C:/Path/To/FMOD_SDK"
cmake --build build
```

### 2. macOS (Clang / Apple Silicon o Intel)

Apri il terminale di macOS e lancia:

```bash
# Configurazione con Release flags (-O3, -ffast-math)
cmake -B build -S . \
      -DFMOD_SDK_DIR="/Users/condivisa/FMOD_Programmer_API/api/core" \
      -DCMAKE_BUILD_TYPE=Release

# Compilazione
cmake --build build --config Release -j$(sysctl -n hw.ncpu)
```

Per generare un binario universale (x86_64 + arm64):
```bash
cmake -B build -S . \
      -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
      -DFMOD_SDK_DIR="/Percorso/FMOD_API" \
      -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

### 3. Linux (GCC o Clang)

```bash
# Configurazione
cmake -B build -S . \
      -DFMOD_SDK_DIR="/opt/fmod/api/core" \
      -DCMAKE_BUILD_TYPE=Release

# Compilazione
cmake --build build --config Release -j$(nproc)
```

### Output dei Binari Compilati

A compilazione ultimata, il file binario della libreria condivisa sarà generato nei seguenti percorsi:

| Sistema Operativo | File Generato | Percorso Standard di Output |
| :--- | :--- | :--- |
| **Windows** | `fmod_square_dsp.dll` | `build/bin/Release/fmod_square_dsp.dll` o `build/Release/fmod_square_dsp.dll` |
| **macOS** | `fmod_square_dsp.dylib` | `build/fmod_square_dsp.dylib` |
| **Linux** | `fmod_square_dsp.so` | `build/fmod_square_dsp.so` |

---

## Installazione e Configurazione in FMOD Studio

FMOD Studio carica i plugin DSP attraverso la sua cartella `Plugins`. Per esporre il plugin grafico nel Mixer Deck:

### 1. Copia dei File del Plugin

Copia il file binario compilato (`.dll`, `.dylib` o `.so`) e il file di metadati XML `fmod_studio/SquareWaveDSP.plugin.xml` nella directory dei plugin.

#### Opzione A: Per Singolo Progetto FMOD Studio (Consigliata)
Nella cartella del tuo progetto FMOD Studio, crea (se non esiste) la cartella `Plugins`:
```text
<MioProgettoFMOD>/
├── MioProgettoFMOD.fspro
├── Assets/
├── Metadata/
└── Plugins/
    ├── fmod_square_dsp.dylib (o .dll / .so)
    └── SquareWaveDSP.plugin.xml
```

#### Opzione B: Installazione Globale per FMOD Studio
- **Windows:** `%LOCALAPPDATA%/FMOD Studio/Plugins/` (oppure `%APPDATA%/FMOD Studio/Plugins/`)
- **macOS:** `~/Library/Application Support/FMOD Studio/Plugins/`
- **Linux:** `~/.local/share/FMOD Studio/Plugins/`

### 2. Caricamento nel Mixer di FMOD Studio

1. **Avvia FMOD Studio** e apri il tuo progetto.
2. Apri la finestra **Mixer Routing** premendo `Ctrl+2` (Windows) o `Cmd+2` (macOS).
3. Seleziona la traccia su cui applicare il generatore (es. una traccia audio dedicata in un evento, un **Audio Bus** di gruppo o il **Master Bus**).
4. Nella vista **Deck** in basso:
   - Fai click con il tasto destro nello spazio vuoto della catena effetti (a destra del Pan o prima del Fader).
   - Seleziona **Add Effect** $\rightarrow$ **Plug-in Effects** $\rightarrow$ **Square Wave Generator**.
5. Vedrai apparire il modulo del plugin nel Deck con i due controlli interattivi:
   - **Manopola "Frequency"**: scala logaritmica da 20.0 Hz a 20,000.0 Hz (default 440.0 Hz).
   - **Fader "Volume"**: scala di guadagno lineare da 0.0 a 1.0 (default 0.20).
6. Premi la barra spaziatrice per avviare il monitoraggio e modulare frequenza e volume in tempo reale.

---

## Integrazione Runtime in C++ / Game Engine

Il seguente listato C++17 è minimale e auto-consistente. Inizializza l'FMOD Core Engine, carica la libreria dinamica tramite `System::loadPlugin`, istanzia il DSP generatore, lo assegna al Master Channel Group e modula la frequenza a runtime.

```cpp
#include <fmod.hpp>
#include <fmod_errors.h>
#include <iostream>
#include <thread>
#include <chrono>

// Helper per il controllo errori FMOD
#define FMOD_CHECK(result) \
    do { \
        FMOD_RESULT r = (result); \
        if (r != FMOD_OK) { \
            std::cerr << "[FMOD Error] " << FMOD_ErrorString(r) \
                      << " at line " << __LINE__ << std::endl; \
            return 1; \
        } \
    } while (0)

int main()
{
    std::cout << "Inizializzazione FMOD Core Engine..." << std::endl;

    FMOD::System* system = nullptr;
    FMOD_CHECK(FMOD::System_Create(&system));

    // Inizializza FMOD con 32 canali virtuali
    FMOD_CHECK(system->init(32, FMOD_INIT_NORMAL, nullptr));

    // 1. Caricamento della libreria dinamica del plugin
#if defined(_WIN32)
    const char* pluginPath = "fmod_square_dsp.dll";
#elif defined(__APPLE__)
    const char* pluginPath = "fmod_square_dsp.dylib";
#else
    const char* pluginPath = "./fmod_square_dsp.so";
#endif

    unsigned int pluginHandle = 0;
    std::cout << "Caricamento plugin: " << pluginPath << std::endl;
    FMOD_CHECK(system->loadPlugin(pluginPath, &pluginHandle, 0));

    // 2. Creazione dell'istanza del DSP tramite l'handle del plugin
    FMOD::DSP* squareWaveDSP = nullptr;
    FMOD_CHECK(system->createDSPByPlugin(pluginHandle, &squareWaveDSP));

    // 3. Connessione del DSP al Master Channel Group per la riproduzione
    FMOD::ChannelGroup* masterGroup = nullptr;
    FMOD_CHECK(system->getMasterChannelGroup(&masterGroup));
    FMOD_CHECK(masterGroup->addDSP(0, squareWaveDSP));

    std::cout << "Generatore di onda quadra attivo!" << std::endl;

    // 4. Modulazione dinamica a runtime dei parametri:
    // Parametro 0: Frequency (Hz)
    // Parametro 1: Volume (0.0 - 1.0)
    const float frequencies[] = { 220.0f, 440.0f, 659.25f, 880.0f }; // Note A3, A4, E5, A5

    for (float freq : frequencies)
    {
        std::cout << "Impostazione frequenza a " << freq << " Hz..." << std::endl;
        FMOD_CHECK(squareWaveDSP->setParameterFloat(0, freq));
        FMOD_CHECK(squareWaveDSP->setParameterFloat(1, 0.15f)); // Volume moderato

        // Aggiorna l'engine audio e attendi 1 secondo per ascoltare il tono
        for (int i = 0; i < 20; ++i)
        {
            FMOD_CHECK(system->update());
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }

    // 5. Cleanup
    std::cout << "Chiusura audio e rilascio risorse..." << std::endl;
    FMOD_CHECK(masterGroup->removeDSP(squareWaveDSP));
    FMOD_CHECK(squareWaveDSP->release());
    FMOD_CHECK(system->unloadPlugin(pluginHandle));
    FMOD_CHECK(system->close());
    FMOD_CHECK(system->release());

    std::cout << "Operazione completata con successo." << std::endl;
    return 0;
}
```

---

## Specifiche dei Parametri

| Indice | Nome Parametro | Tipo | Range | Valore di Default | Mappatura / Scala | Descrizione |
| :---: | :---: | :---: | :---: | :---: | :---: | :--- |
| **0** | `Frequency` | `Float` | `20.0` - `20000.0` | `440.0` | Logaritmica (Hz) | Frequenza fondamentale dell'onda quadra. |
| **1** | `Volume` | `Float` | `0.0` - `1.0` | `0.2` | Lineare / dB | Guadagno d'uscita del segnale sintetizzato. |

### Entry Point per FMOD Core C API
Il plugin esporta con C-linkage l'entry point universale:
```cpp
extern "C" F_EXPORT FMOD_DSP_DESCRIPTION* F_CALL FMODGetDSPDescription();
```
All'avvio, FMOD Core o FMOD Studio invoca questa funzione per interrogare le caratteristiche del DSP, allocare le strutture dei parametri ed eseguire il binding dei callback.
