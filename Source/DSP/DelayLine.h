/*
    DelayLine.h

    Línea de retardo circular muy simple, usada para el "lookahead".

    Idea del lookahead: la decisión de abrir/cerrar el gate se calcula a
    partir del audio ENTRANTE (que aún no ha salido del plugin). El audio
    real que sale del plugin se retrasa unos milisegundos (10-25 ms) para
    que, cuando la ganancia del gate empiece a cerrar, ya se haya "anticipado"
    a la transición y no se coma el ataque de una nota o sílaba.

    El buffer se reserva una única vez para el lookahead MÁXIMO permitido
    (25 ms) durante prepareToPlay, para evitar allocations en tiempo real
    cuando el usuario cambia el parámetro de lookahead.
*/

#pragma once
#include <vector>
#include <algorithm>

class DelayLine
{
public:
    // maxDelaySamples: tamaño máximo reservado (para el lookahead máximo, 25 ms)
    void prepare (int maxDelaySamples)
    {
        bufferSize = std::max (1, maxDelaySamples + 1);
        buffer.assign ((size_t) bufferSize, 0.0f);
        writePos = 0;
    }

    void reset()
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
    }

    // Escribe una muestra nueva y devuelve la muestra retrasada
    // 'delaySamples' atrás (delaySamples debe ser <= maxDelaySamples usado en prepare).
    inline float processSample (float input, int delaySamples) noexcept
    {
        buffer[(size_t) writePos] = input;

        int readPos = writePos - delaySamples;
        if (readPos < 0)
            readPos += bufferSize;

        writePos = (writePos + 1) % bufferSize;

        return buffer[(size_t) readPos];
    }

private:
    std::vector<float> buffer;
    int bufferSize = 1;
    int writePos = 0;
};
