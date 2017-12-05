include ../common.mk

# TODO: a lot of warnings !!!

FLAGS = $(CFLAGS) -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function -Wno-misleading-indentation -fpic -include config.h

OBJ = strutils.o dmesg.o timeutils.o mangle.o monotonic.o

all: clean $(OBJ) 

%.o: %.c
	$(CC) $(FLAGS) -c $< -o $@

clean:
	rm -f $(OBJ)

