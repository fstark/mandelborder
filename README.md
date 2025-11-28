# Mandelbrot Set Explorer

Interactive Mandelbrot set renderer with multiple computation engines and real-time visualization.

## Build

```bash
cd src
make
```

Binary is created in the project root: `../mandelbrot_sdl2`

Dependencies: SDL2, OpenGL 3.2+

## Usage

```bash
./mandelbrot_sdl2 [--engine ENGINE] [--speed] [--verbose] [--auto-zoom] [--pixel-size N]
```

**Options:**
- `--engine`: Choose engine: `border`, `standard`, `simd`, `gpuf`, `gpud` (default: border)
- `--speed`: Enable parallel 4×4 grid mode
- `--verbose`: Show computation stats
- `--auto-zoom`: Automatic zoom exploration
- `--pixel-size N`: Render at reduced resolution (1-20, default: 1)

## Controls

**Keyboard:**
- `SPACE` - Recompute
- `R` - Reset to full set
- `F` - Toggle fast mode (4×4 grid)
- `E` - Cycle engines (Border→Standard→SIMD→GPU-Float→GPU-Double)
- `P` - Random palette
- `V` - Toggle verbose output
- `A` - Toggle auto-zoom
- `X` - Toggle 1×/10× pixel size
- `S` - Save screenshot
- `Shift+S` - Toggle auto-screenshot
- `ESC` - Quit

**Mouse:**
- Drag - Zoom into region
- `Shift`+Drag - Zoom out
- `Ctrl`+Drag - Center-based zoom

## Engines

**Border**: Boundary tracing algorithm - only computes pixels near edges, fills interiors  
**Standard**: Naive per-pixel iteration  
**SIMD**: Vectorized computation (4 pixels parallel)  
**GPU-Float**: OpenGL shader (32-bit precision, ~10× faster)  
**GPU-Double**: OpenGL shader (64-bit precision, slower but deeper zoom)

Fast mode (`--speed` or `F` key): Splits computation across 4×4 grid using threads (not available for GPU engines).

## Verbose Output

With `-v` or `--verbose`, displays computation stats:
```
border  800×600     615.7 ms   -0.5000000000000000   0.0000000000000000     3.00e+00
 simd    4×4     800×600      44.8 ms   -0.5000000000000000   0.0000000000000000     3.00e+00
```
Format: `[engine] [grid] [resolution] [time] [center_real] [center_imag] [diameter]`

## Original Algorithm

Boundary tracing technique by Joel Yliluoma (2010). Original DOS program used VGA Mode 13h; this modernizes it with SDL2/OpenGL.
