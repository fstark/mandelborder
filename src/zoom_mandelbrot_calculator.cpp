#include "zoom_mandelbrot_calculator.h"
#include <algorithm>

ZoomMandelbrotCalculator::ZoomMandelbrotCalculator(int w, int h)
    : width(w), height(h), speedMode(false)
{
    // Default initialization
    updateBounds(-0.5, 0.0, 3.0);
}

void ZoomMandelbrotCalculator::updateBounds(double new_cre, double new_cim, double new_diam)
{
    cre = new_cre;
    cim = new_cim;
    diam = new_diam;
    minr = cre - diam * 0.5 * width / height;
    mini = cim - diam * 0.5;
    maxr = cre + diam * 0.5 * width / height;
    maxi = cim + diam * 0.5;
    stepr = (maxr - minr) / width;
    stepi = (maxi - mini) / height;
}

void ZoomMandelbrotCalculator::updateBoundsExplicit(double new_minr, double new_mini, double new_maxr, double new_maxi)
{
    minr = new_minr;
    mini = new_mini;
    maxr = new_maxr;
    maxi = new_maxi;
    
    // Calculate center and diameter from the explicit bounds
    cre = (minr + maxr) / 2.0;
    cim = (mini + maxi) / 2.0;
    diam = std::max(maxr - minr, maxi - mini);
    
    stepr = (maxr - minr) / width;
    stepi = (maxi - mini) / height;
}
