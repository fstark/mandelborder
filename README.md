
Note: this code was entierly vibe coded from the included PDF and the two following prompts:

1) 

"Can you extract the code from the pdf mandelbrottrace.pdf?"

[led to some issues tryong various approaches, but ultimately worked with a custom python script inline script)

```
~/Development/mandelborder/.venv/bin/python -c "from pypdf import PdfReader; reader = PdfReader('mandelbrotbtrace.pdf'); text = ''.join([page.extract_text() for page in reader.pages]); print(text)"
```

2)

"taking inspiration of this code, write a C++ version that uses SDL2 to compute and display a mandelbrot set"


(command to process a video captured -- nothing to do here)
ffmpeg -i input.mp4 -vf scale=-2:720 -c:v libx264 -preset slow -b:v 2500k -pass 1 -an -f mp4 /dev/null
ffmpeg -i input.mp4 -vf scale=-2:720 -c:v libx264 -preset slow -b:v 2500k -pass 2 -c:a aac -b:a 192k output_720p.mp4




# Mandelbrot Set Renderer

Modern C++ implementation of Mandelbrot set visualization using SDL2, inspired by Joel Yliluoma's boundary tracing algorithm from 2010.

## Features

- **Boundary Tracing Algorithm**: Efficient rendering that only calculates pixels near boundaries
- **Interactive Zoom**: Click and drag to zoom into any region
- **Smooth Color Palette**: Generated using cosine functions for smooth gradients
- **Real-time Rendering**: Updates display during computation

## Algorithm

Instead of computing every pixel individually (which takes WIDTH × HEIGHT iterations), this implementation uses a boundary tracing approach:

1. Start with the edges of the screen
2. For each pixel, check if its neighbors have different iteration counts
3. If they differ, add those neighbors to a queue for processing
4. Continue until the queue is empty
5. Fill any remaining interior pixels with neighbor colors

This is much faster for regions with large uniform areas (like the interior and exterior of the set).

## Requirements

- C++11 or later
- SDL2 library

### Installing SDL2

**macOS (using Homebrew):**
```bash
brew install sdl2
```

**Linux (Ubuntu/Debian):**
```bash
sudo apt-get install libsdl2-dev
```

**Linux (Fedora):**
```bash
sudo dnf install SDL2-devel
```

## Building

```bash
make
```

## Running

```bash
./mandelbrot_sdl2
```

Or combine both steps:
```bash
make run
```

## Controls

- **Mouse**: Click and drag to select a region to zoom into
- **SPACE**: Recompute the current view
- **R**: Reset zoom to full Mandelbrot set view
- **S**: Save screenshot as PNG with timestamp
- **Shift+S**: Toggle auto-screenshot mode (saves every frame)
- **F**: Toggle fast mode (parallel 4x4 grid vs progressive rendering)
- **P**: Change color palette
- **E**: Cycle through render engines (BORDER/STANDARD/SIMD)
- **A**: Toggle auto-zoom mode
- **V**: Toggle verbose mode
- **X**: Toggle pixel size (1x vs 10x for faster rendering)
- **ESC**: Quit

## Customization

You can modify the view parameters in the code:

```cpp
// Default: full set view
double cre = -0.5;
double cim = 0.0;
double diam = 3.0;

// Or try this zoomed detail view:
// double cre = -1.36022;
// double cim = 0.0653316;
// double diam = 0.035;
```

Other parameters you can adjust:
- `WIDTH` and `HEIGHT`: Window resolution (default: 800×600)
- `MAX_ITER`: Maximum iterations (default: 768)
- Color palette in `generatePalette()` method

## Code Structure

- `MandelbrotRenderer` class encapsulates all functionality
- `iterate()`: Core Mandelbrot iteration function
- `scan()`: Boundary tracing logic
- `compute()`: Main computation loop using queue-based algorithm
- `render()`: SDL2 rendering to screen
- Interactive zoom and pan support

## Original Algorithm

This implementation is based on the boundary tracing technique demonstrated by Joel Yliluoma in his 2010 DOS program. The original used VGA Mode 13h graphics; this version modernizes it with SDL2 for cross-platform compatibility.

## License

Inspired by Joel Yliluoma's original work. Feel free to use and modify.
