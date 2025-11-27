#include "standard_mandelbrot_calculator.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <cmath>
#include <algorithm>

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
    // Start high-precision timer
    auto startTime = std::chrono::high_resolution_clock::now();

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

    // Stop timer and calculate elapsed time
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    double milliseconds = duration.count() / 1000.0;
    double seconds = duration.count() / 1000000.0;

    unsigned totalPixels = width * height;
    
    if (verboseMode)
    {
        // Show timing information in verbose mode
        double totalPixelsPerSec = seconds > 0 ? totalPixels / seconds : 0;
        
        std::cout << "Standard Computation complete! Processed " << processed << " / " << totalPixels
                  << " pixels (100.0%)"
                  << " in " << std::fixed << std::setprecision(1) << milliseconds << " ms";
        
        if (speedMode)
        {
            std::cout << " (" << std::fixed << std::setprecision(0) << totalPixelsPerSec << " total px/s)";
        }
        std::cout << std::endl;
    }
}
