CC = gcc
CFLAGS = -Wall -pthread

OBJS_COMMON = ipc_utils.o logs.o

all: symulacja exe_bus exe_passenger exe_cashier

ipc_utils.o: ipc_utils.c ipc_utils.h common.h
	$(CC) $(CFLAGS) -c ipc_utils.c

logs.o: logs.c logs.h
	$(CC) $(CFLAGS) -c logs.c

config.o: config.c config.h
	$(CC) $(CFLAGS) -c config.c

signals.o: signals.c signals.h common.h
	$(CC) $(CFLAGS) -c signals.c

symulacja: main.c signals.o config.o $(OBJS_COMMON)
	$(CC) $(CFLAGS) main.c signals.o config.o $(OBJS_COMMON) -o symulacja

exe_bus: exe_bus.c $(OBJS_COMMON)
	$(CC) $(CFLAGS) exe_bus.c $(OBJS_COMMON) -o exe_bus

exe_passenger: exe_passenger.c $(OBJS_COMMON)
	$(CC) $(CFLAGS) exe_passenger.c $(OBJS_COMMON) -o exe_passenger

exe_cashier: exe_cashier.c $(OBJS_COMMON)
	$(CC) $(CFLAGS) exe_cashier.c $(OBJS_COMMON) -o exe_cashier

clean:
	rm -f *.o symulacja exe_bus exe_passenger exe_cashier symulacja.log