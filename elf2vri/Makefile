BIN=elf2vri

all: $(BIN)

clean:
	rm -f $(BIN)

cleanall:
	rm -f $(BIN) *.o *~

elf2vri: elf2vri.c
	gcc -g -Wall -funsigned-char -O -o $@ $< -lm
	@ls -la $@
