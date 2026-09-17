/*
    PluginEditor.h

    Interfaz gráfica de SilenceFlow.

    Diseño:
      - "Limpieza" es el control principal: un knob grande y centrado.
      - Hold / Ataque / Release en una fila secundaria, más pequeños.
      - Un botón "Avanzado" despliega Threshold y Lookahead (parámetros que
        la mayoría de usuarios no necesitará tocar).
      - Un indicador de actividad (barra horizontal) muestra en tiempo real
        cuándo el gate está cerrando/abriendo, para dar feedback visual sin
        saturar la interfaz.
      - Botón de Bypass arriba a la derecha.
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

//==============================================================================
// Pequeño componente que dibuja una barra de actividad del gate.
// Lee periódicamente el valor atómico publicado por el procesador
// (uiGateGain) y lo interpola visualmente para que se vea suave.
class GateActivityMeter : public juce::Component,
                           private juce::Timer
{
public:
    explicit GateActivityMeter (SilenceFlowAudioProcessor& p) : processor (p)
    {
        startTimerHz (30); // suficiente para feedback visual fluido, barato en CPU
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Fondo
        g.setColour (juce::Colour (0xff1c1f26));
        g.fillRoundedRectangle (bounds, 4.0f);

        // Relleno proporcional a la ganancia actual del gate (0..1).
        auto fillBounds = bounds.reduced (2.0f);
        fillBounds.setWidth (fillBounds.getWidth() * displayedGain);

        juce::Colour fillColour = displayedGain > 0.85f
                                     ? juce::Colour (0xff53d17c)  // abierto: verde
                                     : juce::Colour (0xfff2a154); // cerrando/cerrado: ámbar

        g.setColour (fillColour);
        g.fillRoundedRectangle (fillBounds, 3.0f);

        g.setColour (juce::Colours::white.withAlpha (0.15f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);
    }

private:
    void timerCallback() override
    {
        const float target = processor.uiGateGain.load();
        // Suavizado puramente visual (no afecta al audio), para que la
        // barra no "parpadee" entre refrescos de 30 Hz.
        displayedGain += (target - displayedGain) * 0.35f;
        repaint();
    }

    SilenceFlowAudioProcessor& processor;
    float displayedGain = 1.0f;
};

//==============================================================================
// Slider rotatorio con etiqueta encima y lectura de valor debajo.
// Se usa tanto para el knob grande de Limpieza como para los secundarios.
class LabeledKnob : public juce::Component
{
public:
    LabeledKnob (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID,
                 const juce::String& labelText)
    {
        addAndMakeVisible (slider);
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 20);

        addAndMakeVisible (label);
        label.setText (labelText, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, paramID, slider);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        label.setBounds (bounds.removeFromTop (18));
        slider.setBounds (bounds);
    }

    juce::Slider slider;

private:
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

//==============================================================================
class SilenceFlowAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    explicit SilenceFlowAudioProcessorEditor (SilenceFlowAudioProcessor&);
    ~SilenceFlowAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    SilenceFlowAudioProcessor& audioProcessor;

    // --- Control principal (el más prominente) ---
    LabeledKnob cleanlinessKnob;

    // --- Controles secundarios ---
    LabeledKnob holdKnob;
    LabeledKnob attackKnob;
    LabeledKnob releaseKnob;

    // --- Avanzado (oculto por defecto) ---
    juce::TextButton advancedToggle { "Avanzado \xE2\x96\xBE" }; // "Avanzado ▾"
    LabeledKnob thresholdKnob;
    LabeledKnob lookaheadKnob;
    bool advancedVisible = false;

    // --- Bypass ---
    juce::TextButton bypassButton { "Bypass" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    // --- Medidor de actividad ---
    GateActivityMeter activityMeter;
    juce::Label activityLabel;

    juce::Label titleLabel;

    void updateAdvancedVisibility();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SilenceFlowAudioProcessorEditor)
};
