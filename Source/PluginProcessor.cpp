/*
    PluginProcessor.cpp

    Implementación del motor de audio de SilenceFlow.

    Flujo por bloque (resumen):
      1. Actualizamos los parámetros del SmartGate desde el APVTS.
      2. Para cada sample:
           a) Calculamos el pico absoluto entre canales (envolvente enlazada,
              para que el gate abra/cierre igual en L y R y no rompa la imagen
              estéreo).
           b) Alimentamos el EnvelopeFollower con ese pico -> envolvente suave.
           c) El SmartGate decide la ganancia (0..1) para ESTE instante,
              basándose en la señal ENTRANTE (sin retrasar).
           d) Escribimos la muestra original en la línea de retardo
              (lookahead) y leemos la muestra retrasada correspondiente.
           e) Multiplicamos la muestra retrasada por la ganancia calculada
              en (c). Como la ganancia se calculó a partir de audio "futuro"
              respecto a la muestra que estamos escribiendo, el gate se
              anticipa a los ataques en vez de cortarlos.
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
SilenceFlowAudioProcessor::SilenceFlowAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

SilenceFlowAudioProcessor::~SilenceFlowAudioProcessor() = default;

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout SilenceFlowAudioProcessor::createParameterLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    // --- LIMPIEZA: control principal, el más prominente en la UI. ----------
    // 0% = no hace nada, 100% = limpia casi todos los silencios.
    params.push_back (std::make_unique<AudioParameterFloat>(
        ParameterID { ParamIDs::cleanliness, 1 },
        "Limpieza",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        55.0f, // valor por defecto (parte del preset de voz)
        AudioParameterFloatAttributes().withLabel ("%")));

    // --- HOLD ---------------------------------------------------------------
    params.push_back (std::make_unique<AudioParameterFloat>(
        ParameterID { ParamIDs::hold, 1 },
        "Hold",
        NormalisableRange<float> (0.0f, 500.0f, 1.0f),
        150.0f,
        AudioParameterFloatAttributes().withLabel ("ms")));

    // --- ATAQUE (fade-in) ----------------------------------------------------
    params.push_back (std::make_unique<AudioParameterFloat>(
        ParameterID { ParamIDs::attack, 1 },
        "Ataque",
        NormalisableRange<float> (0.1f, 50.0f, 0.1f, 0.5f), // skew para más resolución en valores bajos
        3.0f,
        AudioParameterFloatAttributes().withLabel ("ms")));

    // --- RELEASE (fade-out) ---------------------------------------------------
    params.push_back (std::make_unique<AudioParameterFloat>(
        ParameterID { ParamIDs::release, 1 },
        "Release",
        NormalisableRange<float> (5.0f, 500.0f, 1.0f, 0.5f),
        120.0f,
        AudioParameterFloatAttributes().withLabel ("ms")));

    // --- THRESHOLD (avanzado / oculto por defecto) ----------------------------
    params.push_back (std::make_unique<AudioParameterFloat>(
        ParameterID { ParamIDs::threshold, 1 },
        "Threshold",
        NormalisableRange<float> (-80.0f, 0.0f, 0.1f),
        -42.0f,
        AudioParameterFloatAttributes().withLabel ("dB")));

    // --- LOOKAHEAD (avanzado, 10-25 ms) ---------------------------------------
    params.push_back (std::make_unique<AudioParameterFloat>(
        ParameterID { ParamIDs::lookahead, 1 },
        "Lookahead",
        NormalisableRange<float> (10.0f, 25.0f, 0.1f),
        15.0f,
        AudioParameterFloatAttributes().withLabel ("ms")));

    // --- BYPASS -----------------------------------------------------------
    params.push_back (std::make_unique<AudioParameterBool>(
        ParameterID { ParamIDs::bypass, 1 },
        "Bypass",
        false));

    return { params.begin(), params.end() };
}

//==============================================================================
float SilenceFlowAudioProcessor::computeEffectiveThresholdDb (float cleanliness0to100, float thresholdDb)
{
    // A 0% de limpieza, el umbral efectivo es extremadamente bajo (-100 dB),
    // por lo que prácticamente ninguna señal cae por debajo y el gate no actúa.
    // A 100% de limpieza, el umbral efectivo es exactamente el valor fijado
    // por el usuario en "Threshold" (modo agresivo de limpieza).
    const float t = juce::jlimit (0.0f, 100.0f, cleanliness0to100) / 100.0f;
    return juce::jmap (t, 0.0f, 1.0f, -100.0f, thresholdDb);
}

//==============================================================================
void SilenceFlowAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);

    currentSampleRate = sampleRate;

    envelopeFollower.prepare (sampleRate);
    smartGate.prepare (sampleRate);

    // Reservamos el buffer de lookahead para el máximo posible (25 ms), así
    // cambiar el parámetro de lookahead en tiempo real no requiere reallocs.
    lookaheadSamplesMax = (int) std::ceil (maxLookaheadMs * 0.001 * sampleRate) + 1;
    for (auto& delay : lookaheadDelays)
        delay.prepare (lookaheadSamplesMax);

    uiGateGain.store (1.0f);
}

void SilenceFlowAudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool SilenceFlowAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Soportamos mono y estéreo, con entrada == salida (efecto simple).
    const auto& mainIn  = layouts.getMainInputChannelSet();
    const auto& mainOut = layouts.getMainOutputChannelSet();

    if (mainIn != mainOut)
        return false;

    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;

    return true;
}
#endif

//==============================================================================
void SilenceFlowAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused (midiMessages);

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    const bool isBypassed = apvts.getRawParameterValue (ParamIDs::bypass)->load() > 0.5f;

    // Si está en bypass, no tocamos el audio pero seguimos "relajando" el
    // indicador de UI hacia 1.0 para que no se quede una barra a medias.
    if (isBypassed)
    {
        uiGateGain.store (1.0f);
        return;
    }

    // --- Leemos parámetros una vez por bloque (barato, evita zipper noise
    //     grave ya que los propios fades del gate suavizan cualquier salto). ---
    const float cleanliness = apvts.getRawParameterValue (ParamIDs::cleanliness)->load();
    const float holdMs      = apvts.getRawParameterValue (ParamIDs::hold)->load();
    const float attackMs    = apvts.getRawParameterValue (ParamIDs::attack)->load();
    const float releaseMs   = apvts.getRawParameterValue (ParamIDs::release)->load();
    const float thresholdDb = apvts.getRawParameterValue (ParamIDs::threshold)->load();
    const float lookaheadMs = apvts.getRawParameterValue (ParamIDs::lookahead)->load();

    const float effectiveThresholdDb = computeEffectiveThresholdDb (cleanliness, thresholdDb);
    smartGate.setParameters (effectiveThresholdDb, holdMs, attackMs, releaseMs);

    const int lookaheadSamples = juce::jlimit (
        0, lookaheadSamplesMax - 1,
        (int) std::round (lookaheadMs * 0.001f * (float) currentSampleRate));

    float lastGain = 1.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        // (a) Pico absoluto entre canales -> envolvente enlazada (estéreo-coherente).
        float peak = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            peak = std::max (peak, std::abs (buffer.getSample (ch, i)));

        // (b) Envolvente suavizada.
        const float envelope = envelopeFollower.processSample (peak);

        // (c) Ganancia del gate para este instante (basada en la señal ENTRANTE).
        const float gain = smartGate.processEnvelope (envelope);
        lastGain = gain;

        // (d) + (e) Retardamos cada canal el lookahead y aplicamos la ganancia
        //     calculada por adelantado, evitando así cortar el ataque.
        for (int ch = 0; ch < numChannels && ch < (int) lookaheadDelays.size(); ++ch)
        {
            const float dry = buffer.getSample (ch, i);
            const float delayed = lookaheadDelays[(size_t) ch].processSample (dry, lookaheadSamples);
            buffer.setSample (ch, i, delayed * gain);
        }
    }

    // Publicamos el último valor de ganancia del bloque para el medidor de la UI.
    uiGateGain.store (lastGain);
}

//==============================================================================
juce::AudioProcessorEditor* SilenceFlowAudioProcessor::createEditor()
{
    return new SilenceFlowAudioProcessorEditor (*this);
}

//==============================================================================
void SilenceFlowAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Guardamos el estado completo del APVTS (todos los parámetros).
    if (auto state = apvts.copyState(); state.isValid())
    {
        if (auto xml = state.createXml())
            copyXmlToBinary (*xml, destData);
    }
}

void SilenceFlowAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
// Esta función la exige JUCE: crea la instancia del plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SilenceFlowAudioProcessor();
}
