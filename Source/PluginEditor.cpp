/*
    PluginEditor.cpp

    Layout de la interfaz. Todo el posicionamiento se hace en resized()
    usando juce::Rectangle para que sea fácil de leer y mantener.
*/

#include "PluginEditor.h"

//==============================================================================
SilenceFlowAudioProcessorEditor::SilenceFlowAudioProcessorEditor (SilenceFlowAudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      cleanlinessKnob (p.apvts, SilenceFlowAudioProcessor::ParamIDs::cleanliness, "LIMPIEZA"),
      holdKnob         (p.apvts, SilenceFlowAudioProcessor::ParamIDs::hold,       "Hold"),
      attackKnob       (p.apvts, SilenceFlowAudioProcessor::ParamIDs::attack,     "Ataque"),
      releaseKnob      (p.apvts, SilenceFlowAudioProcessor::ParamIDs::release,    "Release"),
      thresholdKnob    (p.apvts, SilenceFlowAudioProcessor::ParamIDs::threshold,  "Threshold"),
      lookaheadKnob    (p.apvts, SilenceFlowAudioProcessor::ParamIDs::lookahead,  "Lookahead"),
      activityMeter (p)
{
    // El knob de Limpieza es el foco visual: lo hacemos más grande dándole
    // más espacio en resized() y con una fuente de etiqueta mayor.
    cleanlinessKnob.slider.setColour (juce::Slider::rotarySliderFillColourId,
                                       juce::Colour (0xff53d17c));
    cleanlinessKnob.slider.setColour (juce::Slider::thumbColourId, juce::Colours::white);

    addAndMakeVisible (cleanlinessKnob);
    addAndMakeVisible (holdKnob);
    addAndMakeVisible (attackKnob);
    addAndMakeVisible (releaseKnob);

    addAndMakeVisible (thresholdKnob);
    addAndMakeVisible (lookaheadKnob);
    thresholdKnob.setVisible (false);
    lookaheadKnob.setVisible (false);

    addAndMakeVisible (advancedToggle);
    advancedToggle.onClick = [this]
    {
        advancedVisible = ! advancedVisible;
        updateAdvancedVisibility();
    };

    addAndMakeVisible (bypassButton);
    bypassButton.setClickingTogglesState (true);
    bypassButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffe0555f));
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        p.apvts, SilenceFlowAudioProcessor::ParamIDs::bypass, bypassButton);

    addAndMakeVisible (activityMeter);
    addAndMakeVisible (activityLabel);
    activityLabel.setText ("Actividad del gate", juce::dontSendNotification);
    activityLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    activityLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.6f));
    activityLabel.setJustificationType (juce::Justification::centred);

    addAndMakeVisible (titleLabel);
    titleLabel.setText ("SilenceFlow", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions (22.0f, juce::Font::bold)));
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    titleLabel.setColour (juce::Label::textColourId, juce::Colours::white);

    setResizable (false, false);
    setSize (420, 480);
}

SilenceFlowAudioProcessorEditor::~SilenceFlowAudioProcessorEditor() = default;

//==============================================================================
void SilenceFlowAudioProcessorEditor::updateAdvancedVisibility()
{
    thresholdKnob.setVisible (advancedVisible);
    lookaheadKnob.setVisible (advancedVisible);
    advancedToggle.setButtonText (advancedVisible ? "Avanzado \xE2\x96\xB4" : "Avanzado \xE2\x96\xBE");
    // Redimensionamos la ventana para dejar sitio a los controles avanzados.
    setSize (420, advancedVisible ? 560 : 480);
}

//==============================================================================
void SilenceFlowAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Fondo oscuro simple, look moderno tipo "estudio".
    juce::ColourGradient grad (juce::Colour (0xff20232b), 0.0f, 0.0f,
                                juce::Colour (0xff15171c), 0.0f, (float) getHeight(),
                                false);
    g.setGradientFill (grad);
    g.fillAll();
}

void SilenceFlowAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (16);

    // --- Cabecera: título + bypass ---
    auto header = area.removeFromTop (32);
    bypassButton.setBounds (header.removeFromRight (90));
    titleLabel.setBounds (header);

    area.removeFromTop (10);

    // --- Knob principal de Limpieza (grande, prominente) ---
    auto cleanlinessArea = area.removeFromTop (190);
    cleanlinessKnob.setBounds (cleanlinessArea.withSizeKeepingCentre (180, 190));

    area.removeFromTop (10);

    // --- Medidor de actividad ---
    activityLabel.setBounds (area.removeFromTop (16));
    activityMeter.setBounds (area.removeFromTop (14).reduced (4, 0));

    area.removeFromTop (14);

    // --- Fila secundaria: Hold / Ataque / Release ---
    auto secondaryRow = area.removeFromTop (120);
    const int thirdWidth = secondaryRow.getWidth() / 3;
    holdKnob.setBounds    (secondaryRow.removeFromLeft (thirdWidth).reduced (6));
    attackKnob.setBounds  (secondaryRow.removeFromLeft (thirdWidth).reduced (6));
    releaseKnob.setBounds (secondaryRow.reduced (6));

    area.removeFromTop (10);

    // --- Botón Avanzado ---
    advancedToggle.setBounds (area.removeFromTop (26).withSizeKeepingCentre (140, 26));

    area.removeFromTop (8);

    // --- Fila avanzada: Threshold / Lookahead (solo si está visible) ---
    if (advancedVisible)
    {
        auto advRow = area.removeFromTop (120);
        const int halfWidth = advRow.getWidth() / 2;
        thresholdKnob.setBounds (advRow.removeFromLeft (halfWidth).reduced (10));
        lookaheadKnob.setBounds (advRow.reduced (10));
    }
}
