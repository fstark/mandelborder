#pragma once

#include "zoom_mandelbrot_calculator.h"
#include <vector>

class StorageMandelbrotCalculator : public ZoomMandelbrotCalculator
{
public:
    StorageMandelbrotCalculator(int width, int height);

    const std::vector<int> &getData() const override { return data; }
    void reset() override;

protected:
    std::vector<int> data;
};
