/*
    SmartGate.h

    Este es el corazón "inteligente" de SilenceFlow.

    A diferencia de un noise gate clásico (que compara la señal contra un
    umbral y abre/cierra la ganancia de forma casi instantánea, dando ese
    característico sonido "picado"), SmartGate:

      1) Compara la envolvente (en dB) contra un umbral efectivo.
      2) Exige que la señal permanezca por debajo del umbral durante un
         tiempo mínimo ("Hold") antes de considerar que es un silencio real
         y empezar a cerrar. Esto evita cortar pausas cortas e intencionadas
         (respiraciones, silencios entre frases, golpes de púa, etc.).
      3) Una vez decidido el estado objetivo (abierto/cerrado), la ganancia
         real NUNCA salta de golpe: se desliza hacia el objetivo con un
         filtro de un polo (one-pole), cuyo coeficiente se deriva del tiempo
         de Ataque o Release en milisegundos. Un filtro de un polo produce,
         por construcción, una curva EXPONENCIAL (rápida al principio, luego
         se suaviza) — exactamente la curva "agradable" y no lineal que se
         pide en el encargo, y es extremadamente barato de calcular
         (una resta, una multiplicación, una suma).
*/

#pragma once
#include <cmath>
#include <algorithm>

class SmartGate
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        currentGain = 1.0f;
        holdCounterSamples = 0.0f;
        gateShouldBeOpen = true;
    }

    void reset()
    {
        currentGain = 1.0f;
        holdCounterSamples = 0.0f;
        gateShouldBeOpen = true;
    }

    // Se llama una vez por bloque (o cuando cambien los parámetros) para
    // actualizar los coeficientes internos. Barato, así que no pasa nada
    // si se llama a menudo.
    void setParameters (float thresholdDbIn, float holdMsIn, float attackMsIn, float releaseMsIn)
    {
        thresholdDb = thresholdDbIn;
        holdTargetSamples = (float) (holdMsIn * 0.001 * sr);
        attackCoeff  = timeToCoeff (attackMsIn);
        releaseCoeff = timeToCoeff (releaseMsIn);
    }

    // envelopeLinear: valor de la envolvente (0..~1+) calculado por EnvelopeFollower
    // Devuelve la ganancia (0..1) a aplicar a la muestra retrasada (lookahead) correspondiente.
    inline float processEnvelope (float envelopeLinear) noexcept
    {
        // Convertimos a dB con un piso para evitar -inf en silencio digital puro.
        const float envDb = juceLikeGainToDb (envelopeLinear);

        if (envDb > thresholdDb)
        {
            // Hay señal por encima del umbral: cancelamos cualquier cuenta
            // de "hold" pendiente y el objetivo pasa a ser "abierto".
            holdCounterSamples = 0.0f;
            gateShouldBeOpen = true;
        }
        else
        {
            // Estamos por debajo del umbral: puede ser un silencio real o
            // solo una micro-pausa. Contamos cuánto tiempo lleva así.
            holdCounterSamples += 1.0f;

            if (holdCounterSamples >= holdTargetSamples)
                gateShouldBeOpen = false;
            // Si todavía no se cumple el "Hold", el gate se mantiene abierto
            // (gateShouldBeOpen no cambia), evitando cortar pausas cortas.
        }

        const float target = gateShouldBeOpen ? 1.0f : 0.0f;
        const float coeff  = (target > currentGain) ? attackCoeff : releaseCoeff;

        // Deslizamiento exponencial hacia el objetivo: esto ES el fade
        // suave de entrada/salida pedido en el encargo.
        currentGain += (target - currentGain) * coeff;

        // Protección numérica: evita denormals / valores negativos ínfimos.
        if (currentGain < 1.0e-6f) currentGain = 0.0f;
        if (currentGain > 1.0f)    currentGain = 1.0f;

        return currentGain;
    }

    float getCurrentGain() const noexcept { return currentGain; }

private:
    double sr = 44100.0;

    float thresholdDb = -45.0f;
    float holdTargetSamples = 0.0f;
    float holdCounterSamples = 0.0f;

    float attackCoeff = 0.5f;
    float releaseCoeff = 0.05f;

    float currentGain = 1.0f;
    bool gateShouldBeOpen = true;

    float timeToCoeff (float timeMs) const
    {
        const float t = std::max (0.05f, timeMs) * 0.001f; // mínimo 0.05 ms, evita división por cero
        return 1.0f - std::exp (-1.0f / (t * (float) sr));
    }

    // Conversión lineal -> dB sin depender de juce_audio_basics en este header,
    // con piso de -100 dB para silencio digital.
    static float juceLikeGainToDb (float gain)
    {
        constexpr float floorDb = -100.0f;
        if (gain <= 0.00001f) // ~ -100 dB
            return floorDb;
        return 20.0f * std::log10 (gain);
    }
};
