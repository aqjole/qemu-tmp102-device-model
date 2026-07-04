CC = cc
CFLAGS = -Wall -Wextra -std=c11

OBJS = main.o tmp102_driver.o

all: $(OBJS)

main.o: main.c tmp102_driver.h
	$(CC) $(CFLAGS) -c main.c

tmp102_driver.o: tmp102_driver.c tmp102_driver.h
	$(CC) $(CFLAGS) -c tmp102_driver.c

test_tmp102_driver: test_tmp102_driver.c tmp102_driver.c tmp102_driver.h
	$(CC) $(CFLAGS) -o test_tmp102_driver test_tmp102_driver.c tmp102_driver.c

test: test_tmp102_driver
	./test_tmp102_driver

clean:
	rm -f $(OBJS) test_tmp102_driver
