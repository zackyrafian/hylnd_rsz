CXX      = g++
CXXFLAGS = -shared -fPIC --no-gnu-unique -Wall -g -DWLR_USE_UNSTABLE -std=c++2b -O2
INCLUDES = -Iinclude $(shell pkg-config --cflags pixman-1 libdrm hyprland pangocairo libinput libudev wayland-server xkbcommon)

SRC    = $(wildcard src/*.cpp)
TARGET = build/hbar-overview.so
PATCHED_FLAG = include/hyprland/.patched

all: $(TARGET)

$(PATCHED_FLAG):
	@echo "Patching Hyprland headers for GCC 16 compatibility..."
	@rm -rf include/hyprland
	@cp -r /usr/local/include/hyprland include/hyprland
	@sed -i 's/return std::views::filter(m_workspaces, \[\](const auto& e) { return e; });/return m_workspaces | std::views::filter([](const auto\& e) { return !!e; });/' include/hyprland/src/Compositor.hpp
	@sed -i 's/return l;/return !!l;/g' include/hyprland/src/desktop/view/LayerSurface.hpp
	@sed -i 's/return m_timeline;/return !!m_timeline;/' include/hyprland/src/protocols/DRMSyncobj.hpp
	@mkdir -p $(dir $(PATCHED_FLAG))
	@touch $(PATCHED_FLAG)

$(TARGET): $(PATCHED_FLAG)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SRC) -o $(TARGET)

clean:
	rm -rf build

.PHONY: all clean
