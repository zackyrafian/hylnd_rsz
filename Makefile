CXX      = g++
CXXFLAGS = -shared -fPIC --no-gnu-unique -Wall -g -DWLR_USE_UNSTABLE -std=c++2b -O2
INCLUDES = $(shell pkg-config --cflags pixman-1 libdrm hyprland pangocairo libinput libudev wayland-server xkbcommon)

SRC    = $(wildcard src/*.cpp)
TARGET = build/hbar-overview.so

all: $(TARGET)

$(TARGET):
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SRC) -o $(TARGET)

clean:
	rm -rf build

.PHONY: all clean
