#include "gradient.h"
#include <cmath>
#include <algorithm>
#include <vector>
#include <cstdlib>

// Helper function to clamp values
static Uint8 clamp(double val)
{
    if (val < 0)
        return 0;
    if (val > 255)
        return 255;
    return static_cast<Uint8>(val);
}

std::unique_ptr<Gradient> Gradient::createRandom()
{
    // Randomly choose between Cosine and Polynomial gradients
    std::unique_ptr<Gradient> baseGradient;

    if (rand() % 2 == 0)
    {
        baseGradient = CosineGradient::createRandom();
    }
    else
    {
        baseGradient = PolynomialGradient::createRandom();
    }

    // Randomly decide whether to apply channel swapping (50% chance)
    if (rand() % 2 == 0)
    {
        return ChannelSwapGradient::createRandom(std::move(baseGradient));
    }

    return baseGradient;
}

CosineGradient::CosineGradient(int base, int amplitude, double freqR, double freqG, double freqB)
    : base(base), amplitude(amplitude), freqR(freqR), freqG(freqG), freqB(freqB)
{
}

SDL_Color CosineGradient::getColor(double t) const
{
    // t is 0..1
    // angle = t * 2 * PI

    double r = base - amplitude * std::cos(t * 2 * M_PI * freqR);
    double g = base - amplitude * std::cos(t * 2 * M_PI * freqG);
    double b = base - amplitude * std::cos(t * 2 * M_PI * freqB);

    return {clamp(r), clamp(g), clamp(b), 255};
}

std::unique_ptr<CosineGradient> CosineGradient::createRandom()
{
    // Use prime numbers for frequencies to avoid repeating patterns too quickly
    const std::vector<double> primes = {1.0, 2.0, 3.0, 5.0, 7.0, 11.0, 13.0};

    double freqR = primes[rand() % primes.size()];
    double freqG = primes[rand() % primes.size()];
    double freqB = primes[rand() % primes.size()];

    // Random base and amplitude
    // Ensure base + amplitude <= 255 and base - amplitude >= 0
    // Let's pick amplitude first, say between 50 and 127
    int amplitude = 50 + rand() % 78;

    // Then base must be at least amplitude and at most 255 - amplitude
    int minBase = amplitude;
    int maxBase = 255 - amplitude;
    int base = minBase + rand() % (maxBase - minBase + 1);

    return std::make_unique<CosineGradient>(base, amplitude, freqR, freqG, freqB);
}

PolynomialGradient::PolynomialGradient(double rCoeff, double gCoeff, double bCoeff)
    : rCoeff(rCoeff), gCoeff(gCoeff), bCoeff(bCoeff)
{
}

SDL_Color PolynomialGradient::getColor(double t) const
{
    // Fixed polynomial formulas:
    // r(t) = 9 * (1 - t) * t^3 * 255
    // g(t) = 15 * (1 - t)^2 * t^2 * 255
    // b(t) = 8.5 * (1 - t)^3 * t * 255

    double oneMinusT = 1.0 - t;

    double r = rCoeff * oneMinusT * t * t * t * 255.0;
    double g = gCoeff * oneMinusT * oneMinusT * t * t * 255.0;
    double b = bCoeff * oneMinusT * oneMinusT * oneMinusT * t * 255.0;

    return {clamp(r), clamp(g), clamp(b), 255};
}

std::unique_ptr<PolynomialGradient> PolynomialGradient::createRandom()
{
    // For now, use fixed coefficients
    // Later these will be randomized
    return std::make_unique<PolynomialGradient>(9.0, 15.0, 8.5);
}

ChannelSwapGradient::ChannelSwapGradient(std::unique_ptr<Gradient> gradient, int r, int g, int b)
    : innerGradient(std::move(gradient))
{
    channelMap[0] = r;
    channelMap[1] = g;
    channelMap[2] = b;
}

SDL_Color ChannelSwapGradient::getColor(double t) const
{
    // Get color from inner gradient
    SDL_Color innerColor = innerGradient->getColor(t);

    // Extract channels as array for easy swapping
    Uint8 channels[3] = {innerColor.r, innerColor.g, innerColor.b};

    // Apply channel mapping
    SDL_Color swapped;
    swapped.r = channels[channelMap[0]];
    swapped.g = channels[channelMap[1]];
    swapped.b = channels[channelMap[2]];
    swapped.a = 255;

    return swapped;
}

std::unique_ptr<ChannelSwapGradient> ChannelSwapGradient::createRandom(std::unique_ptr<Gradient> gradient)
{
    // Generate a random permutation of {0, 1, 2}
    std::vector<int> permutation = {0, 1, 2};

    // Fisher-Yates shuffle
    for (int i = 2; i > 0; --i)
    {
        int j = rand() % (i + 1);
        std::swap(permutation[i], permutation[j]);
    }

    return std::make_unique<ChannelSwapGradient>(
        std::move(gradient),
        permutation[0],
        permutation[1],
        permutation[2]);
}
