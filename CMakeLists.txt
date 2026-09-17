cmake_minimum_required(VERSION 3.22)

project(SilenceFlow VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --------------------------------------------------------------------------
# JUCE se espera como subdirectorio (por ejemplo, clonado como submódulo git
# en la carpeta "JUCE" junto a este CMakeLists, o instalado en el sistema).
# Ver README.md para instrucciones de cómo obtenerlo.
# --------------------------------------------------------------------------
add_subdirectory(JUCE)

# En desarrollo local es cómodo que cada build instale el plugin directamente
# en la carpeta de plugins del sistema. En CI (GitHub Actions) eso puede
# fallar por permisos o simplemente no tiene sentido, así que el workflow
# de .github/workflows/build.yml desactiva esto con
# -DSILENCEFLOW_COPY_PLUGIN=OFF y en su lugar sube los binarios como artifact.
option(SILENCEFLOW_COPY_PLUGIN "Copiar el plugin compilado a la carpeta de plugins del sistema" ON)

juce_add_plugin(SilenceFlow
    COMPANY_NAME "TuMarca"                     # <-- cambia esto por tu nombre/empresa
    PLUGIN_MANUFACTURER_CODE Tumc              # 4 caracteres, único por fabricante
    PLUGIN_CODE Sflw                           # 4 caracteres, único por plugin
    FORMATS VST3 AU Standalone                 # AU solo se compila en macOS automáticamente
    PRODUCT_NAME "SilenceFlow"
    COMPANY_WEBSITE ""
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${SILENCEFLOW_COPY_PLUGIN}
    VST3_CATEGORIES "Fx" "Tools"
    AU_MAIN_TYPE kAudioUnitType_Effect
)

# Genera un header con metadatos del proyecto (JucePlugin_Name, etc.)
juce_generate_juce_header(SilenceFlow)

target_sources(SilenceFlow
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
)

target_compile_definitions(SilenceFlow
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(SilenceFlow
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
