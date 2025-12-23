CC = gcc
CFLAGS = -Wall -Wextra -std=gnu99
TARGET = autobus_symulacja
SRCS = main.c ipc_utils.c actors.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) *.o