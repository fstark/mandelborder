#include "grid_mandelbrot_calculator.h"
#include "simd_mandelbrot_calculator.h"
#include "gpu_mandelbrot_calculator.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <cmath>
#include <thread>
#include <vector>
#include <mutex>

GridMandelbrotCalculator::GridMandelbrotCalculator(int w, int h, int rows, int cols)
    : StorageMandelbrotCalculator(w, h), gridRows(rows), gridCols(cols), engineType(EngineType::BORDER)
{
    tileInfos.resize(gridRows * gridCols);

    // Create tile calculators (we'll set their dimensions after calculating geometry)
    tiles.reserve(gridRows * gridCols);

    // Default initialization
    updateBounds(-0.5, 0.0, 3.0);
}

void GridMandelbrotCalculator::calculateTileGeometry()
{
    // Calculate tile dimensions
    // Distribute pixels as evenly as possible across tiles

    for (int row = 0; row < gridRows; ++row)
    {
        for (int col = 0; col < gridCols; ++col)
        {
            int tileIdx = row * gridCols + col;
            TileInfo &tile = tileInfos[tileIdx];

            // Calculate pixel boundaries for this tile
            tile.startX = (col * width) / gridCols;
            tile.startY = (row * height) / gridRows;

            int endX = ((col + 1) * width) / gridCols;
            int endY = ((row + 1) * height) / gridRows;

            tile.width = endX - tile.startX;
            tile.height = endY - tile.startY;

            // Calculate complex plane bounds for this tile
            tile.minR = minr + tile.startX * stepr;
            tile.minI = mini + tile.startY * stepi;
            tile.maxR = minr + endX * stepr;
            tile.maxI = mini + endY * stepi;
        }
    }
}

void GridMandelbrotCalculator::updateBounds(double new_cre, double new_cim, double new_diam)
{
    ZoomMandelbrotCalculator::updateBounds(new_cre, new_cim, new_diam);

    // Calculate geometry for all tiles
    calculateTileGeometry();

    // (Re)create tile calculators with correct dimensions
    tiles.clear();
    for (int i = 0; i < gridRows * gridCols; ++i)
    {
        const TileInfo &tile = tileInfos[i];
        std::unique_ptr<MandelbrotCalculator> calculator;
        
        if (engineType == EngineType::STANDARD) {
            calculator = std::make_unique<StandardMandelbrotCalculator>(tile.width, tile.height);
        } else if (engineType == EngineType::SIMD) {
            calculator = std::make_unique<SimdMandelbrotCalculator>(tile.width, tile.height);
        } else if (engineType == EngineType::GPU) {
            // For GPU, we only want ONE calculator, not a grid.
            // But if we are in this loop, we are creating tiles.
            // We'll handle this by making the GPU calculator only for the first tile 
            // and making others dummy or empty if we are forced to be in a grid.
            // Ideally, GridMandelbrotCalculator should detect GPU mode and not use tiles.
            // For now, let's just create it.
            calculator = std::make_unique<GpuMandelbrotCalculator>(tile.width, tile.height);
        } else {
            calculator = std::make_unique<BorderMandelbrotCalculator>(tile.width, tile.height);
        }

        // Set explicit bounds for this tile (no aspect ratio adjustment)
        calculator->updateBoundsExplicit(tile.minR, tile.minI, tile.maxR, tile.maxI);
        calculator->setSpeedMode(speedMode);

        tiles.push_back(std::move(calculator));
    }
}

void GridMandelbrotCalculator::updateBoundsExplicit(double new_minr, double new_mini, double new_maxr, double new_maxi)
{
    ZoomMandelbrotCalculator::updateBoundsExplicit(new_minr, new_mini, new_maxr, new_maxi);

    // Calculate geometry for all tiles
    calculateTileGeometry();

    // (Re)create tile calculators with correct dimensions
    tiles.clear();
    for (int i = 0; i < gridRows * gridCols; ++i)
    {
        const TileInfo &tile = tileInfos[i];
        std::unique_ptr<MandelbrotCalculator> calculator;
        
        if (engineType == EngineType::STANDARD) {
            calculator = std::make_unique<StandardMandelbrotCalculator>(tile.width, tile.height);
        } else if (engineType == EngineType::SIMD) {
            calculator = std::make_unique<SimdMandelbrotCalculator>(tile.width, tile.height);
        } else if (engineType == EngineType::GPU) {
            calculator = std::make_unique<GpuMandelbrotCalculator>(tile.width, tile.height);
        } else {
            calculator = std::make_unique<BorderMandelbrotCalculator>(tile.width, tile.height);
        }

        // Set explicit bounds for this tile (no aspect ratio adjustment)
        calculator->updateBoundsExplicit(tile.minR, tile.minI, tile.maxR, tile.maxI);
        calculator->setSpeedMode(speedMode);

        tiles.push_back(std::move(calculator));
    }
}

void GridMandelbrotCalculator::reset()
{
    StorageMandelbrotCalculator::reset();
    for (auto &tile : tiles)
    {
        tile->reset();
    }
}

void GridMandelbrotCalculator::setSpeedMode(bool mode)
{
    ZoomMandelbrotCalculator::setSpeedMode(mode);
    for (auto &tile : tiles)
    {
        tile->setSpeedMode(mode);
    }
}

void GridMandelbrotCalculator::compositeData()
{
    // Copy data from all tiles into the unified buffer
    for (int tileIdx = 0; tileIdx < gridRows * gridCols; ++tileIdx)
    {
        const TileInfo &tile = tileInfos[tileIdx];
        const auto &tileData = tiles[tileIdx]->getData();

        // Copy each row of the tile into the unified buffer
        for (int y = 0; y < tile.height; ++y)
        {
            int srcOffset = y * tile.width;
            int dstOffset = (tile.startY + y) * width + tile.startX;

            for (int x = 0; x < tile.width; ++x)
            {
                data[dstOffset + x] = tileData[srcOffset + x];
            }
        }
    }
}

void GridMandelbrotCalculator::compute(std::function<void()> progressCallback)
{
    auto startTime = std::chrono::high_resolution_clock::now();

    unsigned long long totalComposites = 0; // Track how many times we composite

    // GPU engine must run on the main thread (where the GL context is current)
    // So we force sequential mode for GPU.
    if (speedMode && engineType != EngineType::GPU)
    {
        // PARALLEL MODE: Compute all tiles in parallel using threads
        const int numTiles = gridRows * gridCols;
        const unsigned int numThreads = std::thread::hardware_concurrency();
        
        // Lambda to process a range of tiles
        auto processTiles = [this](int startIdx, int endIdx)
        {
            for (int tileIdx = startIdx; tileIdx < endIdx; ++tileIdx)
            {
                tiles[tileIdx]->compute(nullptr);
            }
        };
        
        // Create threads and distribute tiles among them
        std::vector<std::thread> threads;
        int tilesPerThread = (numTiles + numThreads - 1) / numThreads; // Ceiling division
        
        for (unsigned int t = 0; t < numThreads; ++t)
        {
            int startIdx = t * tilesPerThread;
            int endIdx = std::min(startIdx + tilesPerThread, numTiles);
            
            if (startIdx < numTiles)
            {
                threads.emplace_back(processTiles, startIdx, endIdx);
            }
        }
        
        // Wait for all threads to complete
        for (auto &thread : threads)
        {
            thread.join();
        }
    }
    else
    {
        // SEQUENTIAL MODE: Compute tiles one at a time with progressive rendering
        for (int tileIdx = 0; tileIdx < gridRows * gridCols; ++tileIdx)
        {
            auto tileStartTime = std::chrono::high_resolution_clock::now();

            if (verboseMode)
            {
                std::cout << "  Computing tile " << (tileIdx + 1) << "/" << (gridRows * gridCols) << "..." << std::endl;
            }

            // Normal mode: compute with progress callback that composites only this tile and renders
            tiles[tileIdx]->compute([this, tileIdx, progressCallback, &totalComposites]()
                                    {
                // Composite only the current tile's data (not all tiles)
                const TileInfo &tile = tileInfos[tileIdx];
                const auto &tileData = tiles[tileIdx]->getData();
                
                for (int y = 0; y < tile.height; ++y)
                {
                    int srcOffset = y * tile.width;
                    int dstOffset = (tile.startY + y) * width + tile.startX;
                    
                    for (int x = 0; x < tile.width; ++x)
                    {
                        data[dstOffset + x] = tileData[srcOffset + x];
                    }
                }
                
                totalComposites++;
                
                // Trigger the render callback
                if (progressCallback)
                {
                    progressCallback();
                } });

            // After each tile completes in normal mode, composite that tile's final state
            const TileInfo &tile = tileInfos[tileIdx];
            const auto &tileData = tiles[tileIdx]->getData();

            for (int y = 0; y < tile.height; ++y)
            {
                int srcOffset = y * tile.width;
                int dstOffset = (tile.startY + y) * width + tile.startX;

                for (int x = 0; x < tile.width; ++x)
                {
                    data[dstOffset + x] = tileData[srcOffset + x];
                }
            }

            totalComposites++;

            // Trigger a render after each tile completes
            if (progressCallback)
            {
                progressCallback();
            }

            auto tileEndTime = std::chrono::high_resolution_clock::now();
            auto tileDuration = std::chrono::duration_cast<std::chrono::microseconds>(tileEndTime - tileStartTime);
            double tileMs = tileDuration.count() / 1000.0;

            if (verboseMode)
            {
                std::cout << "    Tile " << (tileIdx + 1) << " completed in "
                          << std::fixed << std::setprecision(1) << tileMs << " ms" << std::endl;
            }
        }
    }

    // In speed mode, composite all tiles once at the end
    if (speedMode)
    {
        for (int tileIdx = 0; tileIdx < gridRows * gridCols; ++tileIdx)
        {
            const TileInfo &tile = tileInfos[tileIdx];
            const auto &tileData = tiles[tileIdx]->getData();

            for (int y = 0; y < tile.height; ++y)
            {
                int srcOffset = y * tile.width;
                int dstOffset = (tile.startY + y) * width + tile.startX;

                for (int x = 0; x < tile.width; ++x)
                {
                    data[dstOffset + x] = tileData[srcOffset + x];
                }
            }
        }
        totalComposites = 1; // Only one composite at the end
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    double milliseconds = duration.count() / 1000.0;
    double seconds = duration.count() / 1000000.0;

    if (verboseMode)
    {
        unsigned totalPixels = width * height;
        double pixelsPerSec = seconds > 0 ? totalPixels / seconds : 0;

        std::cout << "Grid computation complete in " << std::fixed << std::setprecision(1)
                  << milliseconds << " ms ("
                  << std::fixed << std::setprecision(0) << pixelsPerSec << " px/s, "
                  << totalComposites << " composites, "
                  << (gridRows * gridCols) << " tiles)" << std::endl;
    }
}

void GridMandelbrotCalculator::setEngineType(EngineType type)
{
    if (engineType != type)
    {
        engineType = type;
        // Re-initialize calculators with new type
        updateBounds(cre, cim, diam);
    }
}

bool GridMandelbrotCalculator::hasOwnOutput() const
{
    // No calculator has own output anymore
    return false;
}

void GridMandelbrotCalculator::render()
{
    // Nothing to do here
}
