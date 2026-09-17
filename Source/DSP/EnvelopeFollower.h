/*
    EnvelopeFollower.h

    Sigue la envolvente de amplitud de la señal usando un filtro de un polo
    (one-pole) con tiempos de ataque y release independientes.

    Esto es lo que permite que SilenceFlow detecte "actividad real" en vez
    de reaccionar a picos instantáneos de sample individuales: un ataque
    rápido capta transitorios (consonantes, ataques de cuerda) mientras que
    un release algo más lento evita que la envolvente "tiemble" entre
    samples silenciosos y no silenciosos.

    Muy barato en CPU: un abs() + una multiplicación + una suma por sample.
*/

#pragma once
#include <juce_dsp/juce_dsp.h>

class EnvelopeFollower
{
public:
    EnvelopeFollower() = default;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        // Tiempos fijos internos: rápidos para no perder ataques, pero
        // suficientes para no captar ruido de alta frecuencia como "señal".
        setTimes (2.0f, 40.0f);
        envelope = 0.0f;
    }

    void setTimes (float attackMs, float releaseMs)
    {
        attackCoeff  = calcCoeff (attackMs);
        releaseCoeff = calcCoeff (releaseMs);
    }

    void reset() { envelope = 0.0f; }

    // Procesa un sample (ya en valor absoluto o el pico de todos los
    // canales, ver processBlock del procesador) y devuelve la envolvente
    // lineal actualizada.
    inline float processSample (float absInput) noexcept
    {
        const float coeff = (absInput > envelope) ? attackCoeff : releaseCoeff;
        envelope += (absInput - envelope) * coeff;
        return envelope;
    }

    float getCurrentValue() const noexcept { return envelope; }

private:
    double sr = 44100.0;
    float envelope = 0.0f;
    float attackCoeff = 0.5f;
    float releaseCoeff = 0.05f;

    // Convierte un tiempo en ms a un coeficiente de suavizado exponencial
    // basado en la constante de tiempo real (curva natural, no lineal).
    float calcCoeff (float timeMs) const
    {
        if (timeMs <= 0.0f)
            return 1.0f;

        const float timeSeconds = timeMs * 0.001f;
        return 1.0f - std::exp (-1.0f / (float) (timeSeconds * sr));
    }
};
