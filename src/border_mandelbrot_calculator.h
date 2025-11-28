#pragma once

#include "storage_mandelbrot_calculator.h"
#include <vector>
#include <functional>

// Boundary-tracing implementation of Mandelbrot calculator
class BorderMandelbrotCalculator : public StorageMandelbrotCalculator
{
public:
    BorderMandelbrotCalculator(int width, int height);

    void compute(std::function<void()> progressCallback) override;
    void reset() override;
    
    std::string getEngineName() const override { return "border"; }

private:
    std::vector<unsigned char> done;
    std::vector<unsigned> queue;
    unsigned queueHead, queueTail;

    enum Flags
    {
        LOADED = 1,
        QUEUED = 2
    };

    int iterate(double x, double y);
    void addQueue(unsigned p);
    int load(unsigned p);
    void scan(unsigned p);
};
