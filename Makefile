CC = x86_64-w64-mingw32-gcc

CFLAGS = -O2 -Wall -Wextra -municode
CPPFLAGS = -Iinclude
LDLIBS = -ladvapi32

TARGET = wsvc-example.exe

SRC = example/main.c src/wsvc.c

all:
	$(CC) $(CFLAGS) $(CPPFLAGS) $(SRC) $(LDLIBS) -o $(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all clean
