#pragma once

#include <vector>
#include <functional>
#include <string>

// Abstract base class for Mandelbrot set calculators
class MandelbrotCalculator
{
public:
    virtual ~MandelbrotCalculator() = default;

    // Core computation interface
    virtual void updateBounds(double cre, double cim, double diam) = 0;
    virtual void updateBoundsExplicit(double minR, double minI, double maxR, double maxI) = 0;
    virtual void compute(std::function<void()> progressCallback) = 0;
    virtual void reset() = 0;

    // Data access
    virtual const std::vector<int> &getData() const = 0;
    virtual int getWidth() const = 0;
    virtual int getHeight() const = 0;

    // View parameters
    virtual double getCre() const = 0;
    virtual double getCim() const = 0;
    virtual double getDiam() const = 0;
    virtual double getMinR() const = 0;
    virtual double getMinI() const = 0;
    virtual double getStepR() const = 0;
    virtual double getStepI() const = 0;

    // Configuration
    virtual void setSpeedMode(bool mode) = 0;
    virtual bool getSpeedMode() const = 0;
    
    // Engine identification for verbose output
    virtual std::string getEngineName() const = 0;

    // Rendering (for GPU implementations)
    virtual bool hasOwnOutput() const { return false; }
    virtual void render() {}

    static constexpr int MAX_ITER = 768;
};
