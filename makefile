CC = gcc
CFLAGS = -Wall -Wextra -std=gnu99
TARGET = autobus_symulacja

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $(TARGET) main.c

clean:
	rm -f $(TARGET) *.o