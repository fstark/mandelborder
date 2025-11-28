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
        std::string engineType = "gpu"; // default to GPU on this branch

        for (int i = 1; i < argc; ++i)
        {
            if (strcmp(argv[i], "--speed") == 0 || strcmp(argv[i], "-s") == 0)
            {
                speedMode = true;
                std::cout << "Speed mode enabled (parallel 4x4 grid)" << std::endl;
            }
            else if (strcmp(argv[i], "--exit") == 0 || strcmp(argv[i], "-e") == 0)
            {
                exitAfterFirstDisplay = true;
                std::cout << "Will exit after first display" << std::endl;
            }
            else if (strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "-v") == 0)
            {
                verboseMode = true;
                std::cout << "Verbose mode enabled" << std::endl;
            }
            else if (strcmp(argv[i], "--engine") == 0)
            {
                if (i + 1 < argc)
                {
                    engineType = argv[++i];
                    std::cout << "Engine type: " << engineType << std::endl;
                }
                else
                {
                    std::cerr << "Error: --engine requires an argument (border|standard|simd|gpu)" << std::endl;
                    return 1;
                }
            }
            else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
            {
                std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
                std::cout << "Options:" << std::endl;
                std::cout << "  --speed, -s         Enable speed mode (parallel computation)" << std::endl;
                std::cout << "  --exit, -e          Exit after first display (for benchmarking)" << std::endl;
                std::cout << "  --verbose, -v       Enable verbose output (timing info)" << std::endl;
                std::cout << "  --engine <type>     Set engine type (border|standard|simd|gpu)" << std::endl;
                std::cout << "  --help, -h          Show this help message" << std::endl;
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
        
        app.run();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
