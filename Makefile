.PHONY: all clean run

CC      = gcc
CXX     = g++
CFLAGS  = -Wall -O2 -mwindows -DUNICODE -D_UNICODE -I.
CXXFLAGS = -Wall -O2 -mwindows -DUNICODE -D_UNICODE -I.
LDFLAGS = -mwindows -lgdi32 -lgdiplus -lcomctl32 -lcomdlg32 -lshell32 -ladvapi32 -lole32 -lstdc++ -luxtheme

SRCS   = helpers.c word_store.c scheduler.c desktop_embed.c render.c tray.c settings.c word_bank.c darkmode.c main.c
OBJS   = $(SRCS:%.c=build/%.o) build/bg_image.o build/resources.o
TARGET = DesktopWord.exe

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LDFLAGS)

build/%.o: %.c main.h bg_image.h darkmode.h | build
	$(CC) -c $< -o $@ $(CFLAGS)

build/bg_image.o: bg_image.cpp bg_image.h | build
	$(CXX) -c $< -o $@ $(CXXFLAGS)

build/resources.o: resources.rc DesktopWord.exe.manifest | build
	windres $< build/resources.o

build:
	mkdir -p build

clean:
	rm -rf build $(TARGET)

run: $(TARGET)
	./$(TARGET)
