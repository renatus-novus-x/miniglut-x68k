TARGET = demo_wirecube.x

CROSS = m68k-xelf-
CC = $(CROSS)gcc
AS = $(CROSS)as
LD = $(CROSS)gcc

CFLAGS = -m68000 -O2 -g -DX68K
CFLAGS += -Iinclude
LDFLAGS =  -lm


SRCS = \
  src/demo_wirecube.c \
  src/miniglut.c \
  src/minigl.c

OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET) $(TARGET).elf

.PHONY: all clean
