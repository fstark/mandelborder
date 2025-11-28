#include "mandelbrot_app.h"
#include <iostream>
#include <cstring>

int main(int argc, char *argv[])
{
    try
    {
        // Parse command line arguments
        bool speedMode = false;
        bool exitAfterFirstDisplay = false;
        bool verboseMode = false;
        bool autoZoom = false;
        bool randomPalette = false;
        int pixelSize = 1;
        std::string engineType = "border"; // default to border tracing

        for (int i = 1; i < argc; ++i)
        {
            if (strcmp(argv[i], "--speed") == 0 || strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--fast") == 0 || strcmp(argv[i], "-f") == 0)
            {
                speedMode = true;
            }
            else if (strcmp(argv[i], "--exit") == 0 || strcmp(argv[i], "-e") == 0)
            {
                exitAfterFirstDisplay = true;
            }
            else if (strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "-v") == 0)
            {
                verboseMode = true;
            }
            else if (strcmp(argv[i], "--auto-zoom") == 0 || strcmp(argv[i], "-a") == 0)
            {
                autoZoom = true;
            }
            else if (strcmp(argv[i], "--random-palette") == 0 || strcmp(argv[i], "-p") == 0)
            {
                randomPalette = true;
            }
            else if (strcmp(argv[i], "--pixel-size") == 0)
            {
                if (i + 1 < argc)
                {
                    pixelSize = std::atoi(argv[++i]);
                    if (pixelSize < 1) pixelSize = 1;
                    if (pixelSize > 20) pixelSize = 20;
                }
                else
                {
                    std::cerr << "Error: --pixel-size requires an argument (1-20)" << std::endl;
                    return 1;
                }
            }
            else if (strcmp(argv[i], "--engine") == 0)
            {
                if (i + 1 < argc)
                {
                    engineType = argv[++i];
                }
                else
                {
                    std::cerr << "Error: --engine requires an argument (border|standard|simd|gpuf|gpud)" << std::endl;
                    return 1;
                }
            }
            else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
            {
                std::cout << "Mandelbrot Set Explorer with Boundary Tracing" << std::endl;
                std::cout << "\nUsage: " << argv[0] << " [options]" << std::endl;
                std::cout << "\nOptions:" << std::endl;
                std::cout << "  --fast, -f, --speed, -s    Enable fast mode (parallel 4x4 grid)" << std::endl;
                std::cout << "  --engine <type>            Set computation engine:" << std::endl;
                std::cout << "                             border   = Boundary tracing (default, fastest)" << std::endl;
                std::cout << "                             standard = Standard pixel-by-pixel" << std::endl;
                std::cout << "                             simd     = SIMD optimized" << std::endl;
                std::cout << "                             gpuf     = GPU float precision (~50ms)" << std::endl;
                std::cout << "                             gpud     = GPU double precision (~550ms)" << std::endl;
                std::cout << "  --pixel-size <1-20>        Set pixel size (1=normal, 10=blocky)" << std::endl;
                std::cout << "  --random-palette, -p       Start with random color palette" << std::endl;
                std::cout << "  --auto-zoom, -a            Enable automatic zooming" << std::endl;
                std::cout << "  --verbose, -v              Enable verbose output (timing info)" << std::endl;
                std::cout << "  --exit, -e                 Exit after first render (benchmarking)" << std::endl;
                std::cout << "  --help, -h                 Show this help message" << std::endl;
                std::cout << "\nKeyboard Controls:" << std::endl;
                std::cout << "  ESC      - Quit (or cancel drag)" << std::endl;
                std::cout << "  SPACE    - Recompute" << std::endl;
                std::cout << "  R        - Reset zoom to full set" << std::endl;
                std::cout << "  F        - Toggle fast mode (parallel computation)" << std::endl;
                std::cout << "  S        - Save screenshot" << std::endl;
                std::cout << "  Shift+S  - Toggle auto-screenshot mode" << std::endl;
                std::cout << "  E        - Cycle engine (Border→Standard→SIMD→GPU-Float→GPU-Double)" << std::endl;
                std::cout << "  P        - Random palette" << std::endl;
                std::cout << "  V        - Toggle verbose mode" << std::endl;
                std::cout << "  A        - Toggle auto-zoom" << std::endl;
                std::cout << "  X        - Toggle pixel size (1x or 10x)" << std::endl;
                std::cout << "\nMouse Controls:" << std::endl;
                std::cout << "  Drag       - Zoom into region" << std::endl;
                std::cout << "  Shift+Drag - Zoom out from region" << std::endl;
                std::cout << "  Ctrl+Drag  - Center-based zoom" << std::endl;
                return 0;
            }
        }

        // Default resolution 800x600
        // Speed mode: 4x4 grid with parallel computation
        // Normal mode: 1x1 grid (single calculator) with progressive rendering
        MandelbrotApp app(800, 600, speedMode, engineType);

        if (exitAfterFirstDisplay)
        {
            app.setExitAfterFirstDisplay(true);
        }

        if (verboseMode)
        {
            app.setVerboseMode(true);
        }

        if (autoZoom)
        {
            app.setAutoZoom(true);
        }

        if (randomPalette)
        {
            app.setRandomPalette();
        }

        if (pixelSize != 1)
        {
            app.setPixelSize(pixelSize);
        }

        app.run();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
