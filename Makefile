CC  ?= gcc
CXX ?= g++
PKG_CONFIG ?= pkg-config
CFLAGS   ?=
CXXFLAGS ?=
LDFLAGS  ?=

LINUX_PACKAGES = libusb-1.0 hidapi-hidraw
WINDOWS_PACKAGES = libusb-1.0 hidapi

.PHONY: all clean

all: steam-haptics-singer

steam-haptics-singer: main.cpp midifile/midifile.c
	$(CC) $(CFLAGS) -c -o midifile.o midifile/midifile.c
	$(CXX) $(CXXFLAGS) -o steam-haptics-singer main.cpp midifile.o $(LDFLAGS) `$(PKG_CONFIG) --libs --cflags $(LINUX_PACKAGES)`

steam-haptics-singer.exe: main.cpp midifile/midifile.c
	$(CC) $(CFLAGS) -c -o midifile.o midifile/midifile.c
	$(CXX) $(CXXFLAGS) -o steam-haptics-singer.exe main.cpp midifile.o $(LDFLAGS) `$(PKG_CONFIG) --libs --cflags $(WINDOWS_PACKAGES)`

steam-haptics-singer-arm64.exe: main.cpp midifile/midifile.c
	clang --target=aarch64-w64-windows-gnu --sysroot=/clangarm64 \
		-c -o midifile.o midifile/midifile.c
	clang++ --target=aarch64-w64-windows-gnu --sysroot=/clangarm64 \
		-stdlib=libc++ \
		-I/clangarm64/include/c++/v1 \
		-I/clangarm64/include \
		-I/clangarm64/include/hidapi \
		-I/clangarm64/include/libusb-1.0 \
		-fuse-ld=lld \
		-L/clangarm64/lib \
		-o steam-haptics-singer.exe main.cpp midifile.o \
		-lusb-1.0 -lhidapi

clean:
	rm -f steam-haptics-singer steam-haptics-singer.exe midifile.o
