CC=	gcc
CFLAGS=	-g -Wall -Werror -std=gnu99 -Iinclude
LD=	gcc
LDFLAGS= -L.
AR=	ar
ARFLAGS= rcs
TARGETS= bin/spidey

all:		$(TARGETS)

clean:
	@echo Cleaning...
	@rm -f $(TARGETS) lib/*.a src/*.o *.log *.input

.PHONY:		all test clean

# TODO: Add rules for bin/spidey, lib/libspidey.a, and any intermediate objects

# Main Program
bin/spidey: src/spidey.o lib/libspidey.a	
	@echo Linking bin/spidey...
	@$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

# Library
lib/libspidey.a: src/forking.o src/handler.o src/request.o src/single.o src/socket.o src/utils.o
	@echo Linking lib/libspidey.a...
	@$(AR) $(ARFLAGS) $@ $^

# Compiling

src/forking.o: src/forking.c
	@echo Compiling src/forking.o...
	@$(CC) $(CFLAGS) -c -o $@ $<
	
src/handler.o: src/handler.c
	@echo Compiling src/handler.o...
	@$(CC) $(CFLAGS) -c -o $@ $<
	
src/request.o: src/request.c
	@echo Compiling src/request.o...
	@$(CC) $(CFLAGS) -c -o $@ $<
	
src/single.o: src/single.c
	@echo Compiling src/single.o...
	@$(CC) $(CFLAGS) -c -o $@ $<
	
src/socket.o: src/socket.c
	@echo Compiling src/socket.o...
	@$(CC) $(CFLAGS) -c -o $@ $<
	
src/utils.o: src/utils.c
	@echo Compiling src/utils.o...
	@$(CC) $(CFLAGS) -c -o $@ $<
	
src/spidey.o: src/spidey.c
	@echo Compiling src/spidey.o...
	@$(CC) $(CFLAGS) -c -o $@ $<
	
