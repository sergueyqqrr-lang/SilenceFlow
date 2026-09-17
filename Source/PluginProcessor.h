/*
    PluginProcessor.h

    Declaración del procesador de audio de SilenceFlow.

    Usa un AudioProcessorValueTreeState (APVTS) para gestionar todos los
    parámetros automatizables de forma segura para host/automatización y
    para poder enlazar sliders del editor sin gestionar manualmente el
    thread-safety.
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>

#include "DSP/EnvelopeFollower.h"
#include "DSP/DelayLine.h"
#include "DSP/SmartGate.h"

//==============================================================================
class SilenceFlowAudioProcessor  : public juce::AudioProcessor
{
public:
    SilenceFlowAudioProcessor();
    ~SilenceFlowAudioProcessor() override;

    //=== AudioProcessor overrides ============================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Voz (por defecto)"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //=== Parámetros ===========================================================
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;

    // IDs de parámetros, centralizados para que el editor no use strings sueltos.
    struct ParamIDs
    {
        static constexpr auto cleanliness = "cleanliness"; // "Limpieza" 0-100 %
        static constexpr auto hold        = "hold";        // ms
        static constexpr auto attack      = "attack";       // ms (velocidad del fade-in)
        static constexpr auto release     = "release";      // ms (velocidad del fade-out)
        static constexpr auto threshold   = "threshold";    // dB (avanzado)
        static constexpr auto lookahead   = "lookahead";    // ms (avanzado, 10-25 ms)
        static constexpr auto bypass      = "bypass";
    };

    // Máximo lookahead soportado (para reservar el buffer una sola vez).
    static constexpr float maxLookaheadMs = 25.0f;

    // Valor de ganancia actual del gate (0..1), publicado para que el
    // editor pueda dibujar el indicador de actividad sin acceder al DSP
    // directamente (thread-safe vía std::atomic).
    std::atomic<float> uiGateGain { 1.0f };

private:
    //=== Estado DSP por canal ================================================
    EnvelopeFollower envelopeFollower;   // envolvente enlazada (mono-sum de canales)
    SmartGate smartGate;                 // decide y suaviza la ganancia del gate
    std::array<DelayLine, 2> lookaheadDelays; // una línea de retardo por canal (máx. estéreo)

    double currentSampleRate = 44100.0;
    int lookaheadSamplesMax = 0;

    // Mapea el parámetro "Limpieza" (0-100 %) + "Threshold" (dB, avanzado)
    // al umbral efectivo en dB que usa el SmartGate.
    //   - Limpieza 0%   -> umbral muy bajo (~-100 dB): el gate casi nunca actúa.
    //   - Limpieza 100% -> umbral = el valor exacto fijado en "Threshold".
    static float computeEffectiveThresholdDb (float cleanliness0to100, float thresholdDb);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SilenceFlowAudioProcessor)
};
