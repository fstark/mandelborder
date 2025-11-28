#include "grid_mandelbrot_calculator.h"
#include "simd_mandelbrot_calculator.h"
#include "gpu_mandelbrot_calculator.h"
#include <format>
#include <thread>
#include <vector>

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

        if (engineType == EngineType::STANDARD)
        {
            calculator = std::make_unique<StandardMandelbrotCalculator>(tile.width, tile.height);
        }
        else if (engineType == EngineType::SIMD)
        {
            calculator = std::make_unique<SimdMandelbrotCalculator>(tile.width, tile.height);
        }
        else if (engineType == EngineType::GPUF)
        {
            // For GPU, we only want ONE calculator, not a grid.
            // But if we are in this loop, we are creating tiles.
            // We'll handle this by making the GPU calculator only for the first tile
            // and making others dummy or empty if we are forced to be in a grid.
            // Ideally, GridMandelbrotCalculator should detect GPU mode and not use tiles.
            // For now, let's just create it.
            calculator = std::make_unique<GpuMandelbrotCalculator>(tile.width, tile.height, GpuMandelbrotCalculator::Precision::FLOAT);
        }
        else if (engineType == EngineType::GPUD)
        {
            calculator = std::make_unique<GpuMandelbrotCalculator>(tile.width, tile.height, GpuMandelbrotCalculator::Precision::DOUBLE);
        }
        else
        {
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

        if (engineType == EngineType::STANDARD)
        {
            calculator = std::make_unique<StandardMandelbrotCalculator>(tile.width, tile.height);
        }
        else if (engineType == EngineType::SIMD)
        {
            calculator = std::make_unique<SimdMandelbrotCalculator>(tile.width, tile.height);
        }
        else if (engineType == EngineType::GPUF)
        {
            calculator = std::make_unique<GpuMandelbrotCalculator>(tile.width, tile.height, GpuMandelbrotCalculator::Precision::FLOAT);
        }
        else if (engineType == EngineType::GPUD)
        {
            calculator = std::make_unique<GpuMandelbrotCalculator>(tile.width, tile.height, GpuMandelbrotCalculator::Precision::DOUBLE);
        }
        else
        {
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
    unsigned long long totalComposites = 0; // Track how many times we composite

    // GPU engine must run on the main thread (where the GL context is current)
    // So we force sequential mode for GPU.
    if (speedMode && engineType != EngineType::GPUF && engineType != EngineType::GPUD)
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

            // After tile completes, composite its final state and render once more
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
            
            // Render the final tile state
            if (progressCallback)
            {
                progressCallback();
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

std::string GridMandelbrotCalculator::getEngineName() const
{
    if (tiles.empty())
        return "unknown";
    
    std::string baseName = tiles[0]->getEngineName();
    
    // Append grid info if grid is larger than 1x1
    if (gridRows > 1 || gridCols > 1)
    {
        return baseName + std::format(" {:>4}x{:<4}", gridRows, gridCols);
    }
    
    return baseName;
}
