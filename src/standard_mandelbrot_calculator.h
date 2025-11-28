#pragma once

#include "storage_mandelbrot_calculator.h"
#include <vector>
#include <functional>

// Standard implementation of Mandelbrot calculator (top-to-bottom)
class StandardMandelbrotCalculator : public StorageMandelbrotCalculator
{
public:
    StandardMandelbrotCalculator(int width, int height);

    void compute(std::function<void()> progressCallback) override;
    
    std::string getEngineName() const override { return "  std"; }

private:
    int iterate(double x, double y);
};
