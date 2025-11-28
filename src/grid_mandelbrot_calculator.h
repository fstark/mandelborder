#pragma once

#include "storage_mandelbrot_calculator.h"
#include "border_mandelbrot_calculator.h"
#include "standard_mandelbrot_calculator.h"
#include <vector>
#include <memory>
#include <functional>

// Grid-based implementation that divides computation into tiles
class GridMandelbrotCalculator : public StorageMandelbrotCalculator
{
public:
    enum class EngineType
    {
        BORDER,
        STANDARD,
        SIMD,
        GPUF, // GPU with float precision
        GPUD  // GPU with double precision
    };

    GridMandelbrotCalculator(int width, int height, int gridRows, int gridCols);

    void updateBounds(double cre, double cim, double diam) override;
    void updateBoundsExplicit(double minR, double minI, double maxR, double maxI) override;
    void compute(std::function<void()> progressCallback) override;
    void reset() override;

    void setSpeedMode(bool mode) override;

    void setEngineType(EngineType type);
    EngineType getEngineType() const { return engineType; }
    
    std::string getEngineName() const override;

    // Override to handle GPU pass-through
    bool hasOwnOutput() const override;
    void render() override;

private:
    int gridRows;
    int gridCols;

    EngineType engineType;

    std::vector<std::unique_ptr<MandelbrotCalculator>> tiles;

    // Helper structures to track tile geometry
    struct TileInfo
    {
        int startX, startY; // Starting pixel position
        int width, height;  // Tile dimensions in pixels
        double minR, minI;  // Complex plane bounds
        double maxR, maxI;
    };
    std::vector<TileInfo> tileInfos;

    void calculateTileGeometry();
    void compositeData();
};
