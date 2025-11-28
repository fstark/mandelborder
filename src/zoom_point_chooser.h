#pragma once

#include <vector>

class ZoomPointChooser
{
public:
    ZoomPointChooser(int width, int height);

    // Find an interesting point to zoom to
    // Returns true if a good point was found, false if falling back to center
    bool findInterestingPoint(const std::vector<int> &data, int maxIter,
                              int &outX, int &outY,
                              int zoomRectWidth, int zoomRectHeight);

private:
    int width;
    int height;

    // Helper to calculate min/max iterations in a rectangle
    void getIterationRange(const std::vector<int> &data, int maxIter,
                           int x, int y, int w, int h,
                           int &outMin, int &outMax);

    // Calculate diversity score for a potential zoom point
    int calculateDiversityScore(const std::vector<int> &data, int maxIter,
                                int centerX, int centerY,
                                int rectWidth, int rectHeight);
};
