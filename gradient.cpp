#include "gradient.h"
#include <cmath>
#include <algorithm>

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

    auto clamp = [](double val) -> Uint8 {
        if (val < 0) return 0;
        if (val > 255) return 255;
        return static_cast<Uint8>(val);
    };

    return {clamp(r), clamp(g), clamp(b), 255};
}
