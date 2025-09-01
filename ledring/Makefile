
SRCS = ledring.c
OBJS = $(SRCS:.c=.o)

CFLAGS = -Wall -O2

TARGETS = ledring

all: $(TARGETS)

ledring: $(OBJS)
	$(CC) -o $@ $^

clean:
	rm -f $(TARGETS) *.o
