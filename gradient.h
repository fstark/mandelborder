#ifndef GRADIENT_H
#define GRADIENT_H

#include <SDL2/SDL.h>

/**
 * Abstract base class for gradients.
 * Returns a color for a value between 0.0 and 1.0.
 */
class Gradient
{
public:
    virtual ~Gradient() = default;

    /**
     * Get the color for a given position t.
     * @param t Position in the gradient (0.0 to 1.0)
     * @return SDL_Color structure with RGBA values
     */
    virtual SDL_Color getColor(double t) const = 0;
};

/**
 * A gradient that uses cosine waves.
 */
class CosineGradient : public Gradient
{
private:
    int base;
    int amplitude;
    double freqR, freqG, freqB;

public:
    CosineGradient(int base, int amplitude, double freqR, double freqG, double freqB);

    SDL_Color getColor(double t) const override;
};

#endif // GRADIENT_H
