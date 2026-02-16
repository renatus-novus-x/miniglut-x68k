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

all: $(TARGET) demo_objflat.x

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

demo_objflat.x: src/demo_objflat.o src/miniglut.o src/minigl.o
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) src/demo_objflat.o $(TARGET) $(TARGET).elf demo_objflat.x demo_objflat.x.elf

.PHONY: all clean
