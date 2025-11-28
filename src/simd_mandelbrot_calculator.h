#pragma once

#include "storage_mandelbrot_calculator.h"

class SimdMandelbrotCalculator : public StorageMandelbrotCalculator
{
public:
    SimdMandelbrotCalculator(int width, int height);

    void compute(std::function<void()> progressCallback) override;
    
    std::string getEngineName() const override { return " simd"; }
};
