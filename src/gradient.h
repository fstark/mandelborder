#ifndef GRADIENT_H
#define GRADIENT_H

#include <SDL2/SDL.h>
#include <memory>

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

    /**
     * Create a random gradient (either Cosine or Polynomial).
     * @return A unique pointer to a randomly generated gradient
     */
    static std::unique_ptr<Gradient> createRandom();
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

    static std::unique_ptr<CosineGradient> createRandom();
};

/**
 * A gradient that uses polynomial functions.
 */
class PolynomialGradient : public Gradient
{
private:
    double rCoeff;
    double gCoeff;
    double bCoeff;

public:
    PolynomialGradient(double rCoeff, double gCoeff, double bCoeff);

    SDL_Color getColor(double t) const override;

    static std::unique_ptr<PolynomialGradient> createRandom();
};

/**
 * A gradient adapter that randomly swaps RGB channels of an underlying gradient.
 */
class ChannelSwapGradient : public Gradient
{
private:
    std::unique_ptr<Gradient> innerGradient;
    int channelMap[3]; // Maps output channels to input channels (e.g., [1,0,2] means R=G, G=R, B=B)

public:
    ChannelSwapGradient(std::unique_ptr<Gradient> gradient, int r, int g, int b);

    SDL_Color getColor(double t) const override;

    static std::unique_ptr<ChannelSwapGradient> createRandom(std::unique_ptr<Gradient> gradient);
};

#endif // GRADIENT_H
