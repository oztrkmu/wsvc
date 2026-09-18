# Linux cross-build: make. Override CC/AR for a native MinGW environment.
CC = x86_64-w64-mingw32-gcc
AR = x86_64-w64-mingw32-ar
CPPFLAGS += -Iinclude -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0600
CFLAGS ?= -O2 -std=c11 -Wall -Wextra -Wpedantic -Werror
LDLIBS += -ladvapi32
BUILD ?= build

all: $(BUILD)/wsvc-example.exe $(BUILD)/libwsvc.a

$(BUILD):
	mkdir -p $@

$(BUILD)/wsvc.o: src/wsvc.c include/wsvc.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/wautorun.o: src/wautorun.c include/wautorun.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/libwsvc.a: $(BUILD)/wsvc.o $(BUILD)/wautorun.o
	$(AR) rcs $@ $^

$(BUILD)/wsvc-example.exe: example/main.c $(BUILD)/libwsvc.a
	$(CC) $(CPPFLAGS) $(CFLAGS) -municode $(LDFLAGS) $< $(BUILD)/libwsvc.a $(LDLIBS) -o $@

clean:
	$(RM) $(BUILD)/wsvc.o $(BUILD)/wautorun.o $(BUILD)/libwsvc.a $(BUILD)/wsvc-example.exe

.PHONY: all clean
