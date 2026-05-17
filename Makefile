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

clean:
	rm -f steam-haptics-singer steam-haptics-singer.exe midifile.o
