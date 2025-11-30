#pragma once

#include "storage_mandelbrot_calculator.h"
#include <vector>
#include <functional>

// Standard implementation of Newton calculator (top-to-bottom)
class StandardNewtonCalculator : public StorageMandelbrotCalculator {
public:
    StandardNewtonCalculator(int width, int height);

    void compute(std::function<void()> progressCallback) override;
    
    std::string getEngineName() const override { return "  cubic newton std"; }

private:
    int iterate(double x, double y);
};
