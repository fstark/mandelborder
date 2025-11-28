# Makefile for Mandelbrot SDL2 renderer

MAKEFLAGS += -j 8

CXX = g++
CXXFLAGS = -std=c++23 -Wall -O3 -march=native -ftree-vectorize $(shell sdl2-config --cflags)
CXXFLAGS_DEBUG = -std=c++23 -Wall -O0 -g $(shell sdl2-config --cflags)
LDFLAGS = $(shell sdl2-config --libs)

# Platform specific flags
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    LDFLAGS += -lGL -lm
endif
ifeq ($(UNAME_S),Darwin)
    LDFLAGS += -framework OpenGL
endif

TARGET = mandelbrot_sdl2
SOURCES = main.cpp mandelbrot_app.cpp border_mandelbrot_calculator.cpp standard_mandelbrot_calculator.cpp grid_mandelbrot_calculator.cpp zoom_point_chooser.cpp gradient.cpp zoom_mandelbrot_calculator.cpp storage_mandelbrot_calculator.cpp simd_mandelbrot_calculator.cpp gpu_mandelbrot_calculator.cpp
OBJS = $(SOURCES:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

debug: CXXFLAGS = $(CXXFLAGS_DEBUG)
debug: clean $(TARGET)

clean:
	rm -f $(TARGET) $(OBJS)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run debug
