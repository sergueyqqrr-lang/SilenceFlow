# SilenceFlow

Gate inteligente y ligero que limpia silencios de forma natural mediante
fades exponenciales suaves, en lugar de cortes duros. Pensado para voz,
guitarra acústica, bajo y grabaciones caseras.

## Estructura del proyecto

```
SilenceFlow/
├── CMakeLists.txt
├── SilenceFlow.jucer        (opcional, si prefieres Projucer — ver abajo)
├── Source/
│   ├── PluginProcessor.h/.cpp   -> motor de audio
│   ├── PluginEditor.h/.cpp      -> interfaz gráfica
│   └── DSP/
│       ├── EnvelopeFollower.h   -> detección de actividad (envolvente)
│       ├── SmartGate.h          -> lógica de umbral + hold + fades exponenciales
│       └── DelayLine.h          -> línea de retardo para el lookahead
└── README.md
```

## Cómo funciona (resumen técnico)

1. **Envolvente enlazada estéreo**: en cada sample se toma el pico absoluto
   entre canales y se suaviza con un `EnvelopeFollower` (ataque ~2 ms,
   release ~40 ms) para obtener una medida de "actividad real", no solo un
   pico instantáneo.
2. **Umbral efectivo**: el knob "Limpieza" (0-100%) interpola entre un
   umbral casi inaudible (-100 dB, el gate no actúa) y el valor del
   parámetro avanzado "Threshold" (modo agresivo).
3. **Hold**: si la envolvente cae por debajo del umbral, se empieza a
   contar tiempo; solo si permanece así más tiempo del fijado en "Hold" se
   considera un silencio real y se inicia el cierre. Esto evita cortar
   pausas cortas intencionadas.
4. **Fades exponenciales**: la ganancia del gate nunca salta — se desliza
   hacia 0 o hacia 1 mediante un filtro de un polo cuyo coeficiente deriva
   de "Ataque"/"Release" en ms. Un filtro de un polo produce, por
   construcción, una curva exponencial suave.
5. **Lookahead (10-25 ms)**: la decisión de ganancia se calcula sobre el
   audio entrante en tiempo real, pero el audio de salida se retrasa unos
   milisegundos mediante `DelayLine`. Así el gate se "anticipa" y no se come
   el ataque de una sílaba o nota.

El preset por defecto de fábrica (Limpieza 55%, Hold 150 ms, Ataque 3 ms,
Release 120 ms, Threshold -42 dB, Lookahead 15 ms) ya está ajustado para
**voz**.

---

## Compilación automática (GitHub Actions)

Este repositorio incluye `.github/workflows/build.yml`, que compila
SilenceFlow automáticamente en **Windows** y **macOS** en cada `push` a
`main`, en cada pull request, y también se puede lanzar a mano desde la
pestaña **Actions** del repositorio (botón "Run workflow").

El workflow:
1. Descarga el código del repositorio.
2. Descarga JUCE (versión fijada, actualmente `8.0.14`) en una carpeta
   `JUCE/` junto al proyecto — no hace falta que la subas tú ni que la
   añadas como submódulo.
3. Configura y compila con CMake en modo `Release` (en macOS genera un
   binario universal Intel + Apple Silicon).
4. Sube el VST3 (Windows/macOS), el AU (macOS) y la versión Standalone como
   **artifacts** descargables desde la propia ejecución del workflow en
   GitHub (pestaña Actions → la ejecución → "Artifacts" al final de la
   página).

No necesitas configurar nada para que funcione; si en el futuro quieres
actualizar la versión de JUCE usada en CI, solo tienes que cambiar el valor
de `JUCE_TAG` al principio de `.github/workflows/build.yml`.

## Requisitos previos

- **JUCE** (versión 7 u 8, la más reciente estable): https://juce.com/get-juce
- **CMake ≥ 3.22** (si usas la vía CMake)
- **Windows**: Visual Studio 2022 (con el workload "Desktop development with C++")
- **macOS**: Xcode ≥ 14 con las Command Line Tools instaladas

---

## Opción A — Compilar con CMake (recomendado, multiplataforma)

1. Clona JUCE dentro de la carpeta del proyecto (o referencia una copia ya
   instalada en tu sistema):

   ```bash
   cd SilenceFlow
   git clone --branch master --depth 1 https://github.com/juce-framework/JUCE.git
   ```

   > Si ya tienes JUCE instalado en otro sitio, puedes en su lugar cambiar
   > `add_subdirectory(JUCE)` en `CMakeLists.txt` por la ruta absoluta a tu
   > copia, o usar `-DCMAKE_PREFIX_PATH=/ruta/a/JUCE` al configurar.

2. Configura el proyecto:

   ```bash
   cmake -B build -DCMAKE_BUILD_TYPE=Release
   ```

   En Windows con Visual Studio, puedes generar el `.sln` directamente:

   ```bash
   cmake -B build -G "Visual Studio 17 2022" -A x64
   ```

3. Compila:

   ```bash
   cmake --build build --config Release
   ```

4. Con `COPY_PLUGIN_AFTER_BUILD TRUE` (ya activado en el `CMakeLists.txt`),
   el VST3/AU se instalará automáticamente en la carpeta de plugins del
   sistema:

   - **Windows**: `C:\Program Files\Common Files\VST3\SilenceFlow.vst3`
   - **macOS VST3**: `~/Library/Audio/Plug-Ins/VST3/SilenceFlow.vst3`
   - **macOS AU**: `~/Library/Audio/Plug-Ins/Components/SilenceFlow.component`

   También se genera una versión **Standalone** (ejecutable independiente),
   útil para probar el plugin sin abrir una DAW.

5. Abre tu DAW, re-escanea plugins si hace falta, y busca **SilenceFlow**
   en la categoría de efectos ("Fx" / "Tools").

   > En macOS, si el AU no aparece, ejecuta `auval -v aufx Sflw Tumc` en una
   > terminal para forzar la validación/registro, o reinicia `killall -9
   > AudioComponentRegistrar`.

---

## Opción B — Compilar con Projucer

Si prefieres el flujo clásico de JUCE con Projucer en vez de CMake puro:

1. Abre **Projucer** (incluido en la descarga de JUCE) y crea un nuevo
   proyecto de tipo **Audio Plug-In**.
2. Ponle de nombre `SilenceFlow` y configura:
   - **Plugin Formats**: VST3, AU (macOS), Standalone
   - **Plugin Characteristics**: desmarca "Plugin is a Synth", desmarca
     "Plugin MIDI Input/Output"
   - **Company Name / Manufacturer Code / Plugin Code**: los que prefieras
     (deben coincidir con lo usado en `CMakeLists.txt` si migras entre
     ambos flujos).
3. En el panel **Modules**, añade (si no están ya): `juce_audio_utils`,
   `juce_audio_processors`, `juce_dsp`, `juce_gui_basics` (y sus
   dependencias habituales: `juce_core`, `juce_events`, `juce_graphics`,
   `juce_data_structures`, `juce_audio_basics`, `juce_audio_devices`,
   `juce_audio_formats`).
4. Borra los archivos `PluginProcessor.h/.cpp` y `PluginEditor.h/.cpp` que
   genera Projucer por defecto, y en su lugar arrastra/copia dentro de la
   carpeta `Source/` de tu proyecto Projucer los archivos de esta entrega
   (incluida la subcarpeta `DSP/`), asegurándote de que aparecen en el
   árbol de "Source" dentro de Projucer (botón derecho → *Add Existing
   Files...* si no se detectan automáticamente).
5. Pulsa **Save and Open in IDE** (icono según plataforma: Visual Studio en
   Windows, Xcode en macOS) y compila el target "Release".

---

## Notas de rendimiento

- El procesamiento por sample usa únicamente operaciones aritméticas
  simples (multiplicaciones, sumas, un `log10`/`exp` por sample para la
  conversión dB), sin allocations en el hilo de audio y sin locks.
- El buffer de lookahead se reserva una única vez en `prepareToPlay` para
  el máximo de 25 ms, por lo que cambiar el parámetro "Lookahead" en tiempo
  real no provoca reallocations.
- Funciona igual de bien en mono (1 canal) y estéreo (2 canales, con
  detección de silencio enlazada entre ambos canales para mantener la
  imagen estéreo intacta).

## Personalización rápida

- Cambia `COMPANY_NAME`, `PLUGIN_MANUFACTURER_CODE` y `PLUGIN_CODE` en
  `CMakeLists.txt` antes de distribuir el plugin (los códigos de 4
  caracteres deben ser únicos para evitar colisiones con otros plugins).
- El preset de voz por defecto se define directamente como valor inicial
  de cada parámetro en `PluginProcessor::createParameterLayout()`
  (`Source/PluginProcessor.cpp`); ahí puedes ajustar los valores de fábrica.
