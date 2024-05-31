CC ?= $(CROSS_COMPILE)gcc
CFLAGS	?= -g -Werror -Wall
LDFLAGS ?= -pthread -lrt
TARGET ?= aesdsocket

all:
	$(CC) $(CFLAGS) $(LDFLAGS)  -o $(TARGET) $(TARGET).c

clean:
	rm -f $(TARGET)

