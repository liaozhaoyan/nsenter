LUA ?= lua5.1
LUA_PC ?= lua5.1
LUA_CFLAGS = $(shell pkg-config $(LUA_PC) --cflags)

CFLAGS ?= -O3

all: nsenter.so

%.o: %.c
	$(CC) -c $(CFLAGS) -fPIC $(LUA_CFLAGS) -o $@ $<

nsenter.so: nsenter.o
	$(CC) -shared nsenter.o -o $@

clean:
	rm -f nsenter.so nsenter.o *.rock