#include "storage_mandelbrot_calculator.h"
#include <algorithm>

StorageMandelbrotCalculator::StorageMandelbrotCalculator(int w, int h)
    : ZoomMandelbrotCalculator(w, h)
{
    data.resize(width * height, MAX_ITER);
}

void StorageMandelbrotCalculator::reset()
{
    std::fill(data.begin(), data.end(), MAX_ITER);
}
