IDIR=./include
CC=gcc
CFLAGS=-I $(IDIR)

ODIR =./build
XDIR =./bin
RDIR =./rsrc

DEPS = ./include/utfconverter.h

OBJ = ./build/utfconverter.o

$(ODIR)/%.o: ./src/%.c $(DEPS)
	mkdir -p bin build
	$(CC) -Wall -Werror -Wextra -pedantic -c -g -o $@ $< $(CFLAGS)

all: $(OBJ)
	gcc -Wall -Werror -Wextra -pedantic -g -o $(XDIR)/utf $^ $(CFLAGS) -lm
	
debug: all
 
.PHONY: clean

clean:
	rm -f $(ODIR)/*.o utf
	rm -rf bin build
