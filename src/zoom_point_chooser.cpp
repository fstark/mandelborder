#include "zoom_point_chooser.h"
#include <algorithm>
#include <cstdlib>

ZoomPointChooser::ZoomPointChooser(int w, int h)
    : width(w), height(h)
{
}

void ZoomPointChooser::getIterationRange(const std::vector<int> &data, int maxIter,
                                         int x, int y, int w, int h,
                                         int &outMin, int &outMax)
{
    outMin = maxIter;
    outMax = 0;

    // Clamp rectangle to screen bounds
    int x1 = std::max(0, x);
    int y1 = std::max(0, y);
    int x2 = std::min(width, x + w);
    int y2 = std::min(height, y + h);

    for (int py = y1; py < y2; ++py)
    {
        for (int px = x1; px < x2; ++px)
        {
            int p = py * width + px;
            int iter = data[p];

            if (iter < maxIter)
            {
                if (iter < outMin)
                    outMin = iter;
                if (iter > outMax)
                    outMax = iter;
            }
        }
    }
}

int ZoomPointChooser::calculateDiversityScore(const std::vector<int> &data, int maxIter,
                                              int centerX, int centerY,
                                              int rectWidth, int rectHeight)
{
    int x = centerX - rectWidth / 2;
    int y = centerY - rectHeight / 2;

    int minIter, maxIter_;
    getIterationRange(data, maxIter, x, y, rectWidth, rectHeight, minIter, maxIter_);

    // Score is based on:
    // 1. Range of iterations (diversity)
    // 2. Maximum iteration value (we want high complexity)
    // Combined score: range * maxIter
    int range = maxIter_ - minIter;
    return range * maxIter_;
}

bool ZoomPointChooser::findInterestingPoint(const std::vector<int> &data, int maxIter,
                                            int &outX, int &outY,
                                            int zoomRectWidth, int zoomRectHeight)
{
    // First pass: find the maximum non-MAX_ITER value in the entire view
    int maxIterFound = 0;
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            int p = y * width + x;
            int iter = data[p];

            if (iter < maxIter && iter > maxIterFound)
            {
                maxIterFound = iter;
            }
        }
    }

    // If we found no valid iterations, fall back to center
    if (maxIterFound == 0)
    {
        outX = width / 2;
        outY = height / 2;
        return false;
    }

    // Define candidates as points with high iteration values
    int threshold = maxIterFound - 5;

    // Second pass: use reservoir sampling to pick up to 100 candidate points
    // This limits scoring to a fixed number regardless of how many pixels qualify
    struct Point
    {
        int x, y;
    };
    std::vector<Point> sampledPoints;
    sampledPoints.reserve(100);
    
    int count = 0;
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            int p = y * width + x;
            int iter = data[p];

            // Point is a candidate if it has high iteration but not max
            if (iter >= threshold && iter < maxIter)
            {
                count++;
                if (sampledPoints.size() < 100)
                {
                    // Fill reservoir
                    sampledPoints.push_back({x, y});
                }
                else
                {
                    // Randomly replace with decreasing probability
                    int j = rand() % count;
                    if (j < 100)
                    {
                        sampledPoints[j] = {x, y};
                    }
                }
            }
        }
    }
    
    // If no candidates found, fall back to center
    if (sampledPoints.empty())
    {
        outX = width / 2;
        outY = height / 2;
        return false;
    }

    // Score the sampled points
    struct Candidate
    {
        int x, y;
        int score;
    };
    std::vector<Candidate> candidates;
    
    for (const auto& point : sampledPoints)
    {
        int score = calculateDiversityScore(data, maxIter, point.x, point.y,
                                            zoomRectWidth, zoomRectHeight);
        if (score > 0)
        {
            candidates.push_back({point.x, point.y, score});
        }
    }

    // If we found good candidates, pick one randomly from the top scorers
    if (!candidates.empty())
    {
        // Sort by score (descending)
        std::sort(candidates.begin(), candidates.end(),
                  [](const Candidate &a, const Candidate &b)
                  { return a.score > b.score; });

        // Pick randomly from top 20% of candidates (or at least top 1)
        int topCount = std::max(1, (int)(candidates.size() * 0.2));
        int idx = rand() % topCount;

        outX = candidates[idx].x;
        outY = candidates[idx].y;
        return true;
    }

    // Fallback to center if no good candidates found
    outX = width / 2;
    outY = height / 2;
    return false;
}
