# Makefile for Mandelbrot SDL2 renderer

MAKEFLAGS += -j 8

CXX = g++
CXXFLAGS = -std=c++14 -Wall -O3 $(shell sdl2-config --cflags)
LDFLAGS = $(shell sdl2-config --libs)

# Linux specific flags
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    LDFLAGS += -lm
endif

TARGET = mandelbrot_sdl2
SOURCES = main.cpp mandelbrot_app.cpp mandelbrot_calculator.cpp zoom_point_chooser.cpp gradient.cpp
OBJS = $(SOURCES:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
