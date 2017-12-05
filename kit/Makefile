
include ../common.mk

CC = gcc -std=gnu99
CFLAGS = -g -O0 -Wall -Wextra -DLUA_COMPAT_5_2 -DLKTKIT -I. -I../linux-api -I../lua
CORE_O = lapi.o lcode.o lctype.o ldebug.o ldo.o ldump.o lfunc.o lgc.o llex.o \
    lmem.o lobject.o lopcodes.o lparser.o lstate.o lstring.o ltable.o \
    ltm.o lundump.o lvm.o lzio.o
LIB_O = lauxlib.o lbaselib.o lbitlib.o lcorolib.o ldblib.o liolib.o \
    lmathlib.o loslib.o lstrlib.o ltablib.o lutf8lib.o loadlib.o linit.o

 

lktk:
	#ifeq ($(WITHOUT_READLINE),1)
	make -C ../lua MYCFLAGS='-I. -DWITHOUT_READLINE=1'
	$(CC) -c $(CFLAGS) -DLUA_USE_LINUX -DWITHOUT_READLINE=1 lktkassert.c
	$(CC) -c $(CFLAGS) -DLUA_USE_LINUX -DWITHOUT_READLINE=1 lktklib.c
	$(CC) -c $(CFLAGS) -DLUA_USE_LINUX -DWITHOUT_READLINE=1 lktk.c
	$(CC) -o lktk ../lua/liblua.a \
		$(foreach f,$(CORE_O), ../lua/$(f)) \
		$(foreach f,$(LIB_O), ../lua/$(f)) \
		lktklib.o lktkassert.o lktk.o -Wl,-E -ldl -lm 
	#else
	#$(MAKE) $(ALL) SYSCFLAGS="-DLUA_USE_LINUX" SYSLIBS="-Wl,-E -ldl -lreadline"
	#endif

clean:
	$(MAKE) -C ../lua clean
	$(RM) lktk *.o 

