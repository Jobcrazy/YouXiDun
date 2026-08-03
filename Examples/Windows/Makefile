CC := D:/MinGW32/bin/gcc.exe
WINDRES := D:/MinGW32/bin/windres.exe

TARGET := Release/WinDemo.exe
SOURCES := src/main.c src/tcp_client.c src/shield_sdk.c
OBJECTS := $(SOURCES:src/%.c=build/%.o)
RESOURCE_OBJECT := build/resource.o
CPPFLAGS := -Iinclude -Ires -D_WIN32_WINNT=0x0601
CFLAGS := -std=c11 -Os -flto -finput-charset=UTF-8 -fdata-sections -ffunction-sections -Wall -Wextra -Wpedantic
LDLIBS := -lws2_32 -lcomctl32 -lgdi32
LDFLAGS := -mwindows -flto -s -Wl,--gc-sections

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS) $(RESOURCE_OBJECT) | Release
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS) $(RESOURCE_OBJECT) $(LDLIBS)

build/%.o: src/%.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(RESOURCE_OBJECT): res/resource.rc res/resource.h res/Shield.ico | build
	$(WINDRES) -Ires res/resource.rc -O coff -o $@

build Release:
	mkdir $@

clean:
	-if exist build\*.o del /Q build\*.o
	-if exist Release\WinDemo.exe del /Q Release\WinDemo.exe
