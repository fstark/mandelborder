#include "standard_mandelbrot_calculator.h"
#include <cmath>

StandardMandelbrotCalculator::StandardMandelbrotCalculator(int w, int h)
    : StorageMandelbrotCalculator(w, h)
{
}

int StandardMandelbrotCalculator::iterate(double x, double y)
{
    double r = x, i = y;
    int iter;

    for (iter = 0; iter < MAX_ITER; ++iter)
    {
        double r2 = r * r;
        double i2 = i * i;

        if (r2 + i2 >= 4.0)
            break;

        double ri = r * i;
        i = ri + ri + y; // z = z^2 + c
        r = r2 - i2 + x;
    }

    return iter;
}

void StandardMandelbrotCalculator::compute(std::function<void()> progressCallback)
{
    unsigned processed = 0;
    
    for (int y = 0; y < height; ++y)
    {
        double cy = mini + y * stepi;
        for (int x = 0; x < width; ++x)
        {
            double cx = minr + x * stepr;
            data[y * width + x] = iterate(cx, cy);
            processed++;
        }

        // Update display periodically (skip in speed mode)
        if (!speedMode && processed % (width * 10) == 0) // Update every 10 lines
        {
            if (progressCallback)
                progressCallback();
        }
    }

}
