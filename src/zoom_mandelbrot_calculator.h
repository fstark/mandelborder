#pragma once

#include "mandelbrot_calculator.h"

class ZoomMandelbrotCalculator : public MandelbrotCalculator
{
public:
    ZoomMandelbrotCalculator(int width, int height);

    void updateBounds(double cre, double cim, double diam) override;
    void updateBoundsExplicit(double minR, double minI, double maxR, double maxI) override;

    int getWidth() const override { return width; }
    int getHeight() const override { return height; }

    double getCre() const override { return cre; }
    double getCim() const override { return cim; }
    double getDiam() const override { return diam; }
    double getMinR() const override { return minr; }
    double getMinI() const override { return mini; }
    double getStepR() const override { return stepr; }
    double getStepI() const override { return stepi; }

    void setSpeedMode(bool mode) override { speedMode = mode; }
    bool getSpeedMode() const override { return speedMode; }

protected:
    int width;
    int height;

    double cre, cim, diam;
    double minr, mini, maxr, maxi;
    double stepr, stepi;

    bool speedMode;
};
