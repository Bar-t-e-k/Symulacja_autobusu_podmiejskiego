CC = gcc
CFLAGS = -Wall -Wextra -std=gnu99 -D_GNU_SOURCE

COMMON_OBJS = ipc_utils.o actors.o logs.o signals.o config.o

all: symulacja exe_bus exe_passenger exe_cashier

symulacja: main.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o symulacja main.o $(COMMON_OBJS)

exe_bus: exe_bus.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o exe_bus exe_bus.o $(COMMON_OBJS)

exe_passenger: exe_passenger.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o exe_passenger exe_passenger.o $(COMMON_OBJS)

exe_cashier: exe_cashier.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o exe_cashier exe_cashier.o $(COMMON_OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f symulacja exe_bus exe_passenger exe_cashier *.o symulacja.log