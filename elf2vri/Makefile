# Makefile voor elf2vri
# Gecompileerd op: 20 juli 2025

# Variabelen
BIN = elf2vri
SRC = elf2vri.c
OBJ = $(SRC:.c=.o)
CC = gcc

BUILD ?= DEBUG

CFLAGS_DEBUG = -g -Wextra -Wpedantic -Og
CFLAGS_RELEASE = -O2

CFLAGS = $(CFLAGS_$(BUILD))
LDFLAGS = -lm
RM = rm -f

# Standaarddoel
all: $(BIN)

# Linken van objectbestanden naar uitvoerbaar bestand
$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDFLAGS)
	@echo "Gebouwd: $@"

# Compilatie van broncode naar objectbestanden
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Schoonmaak van gegenereerde bestanden
clean:
	$(RM) $(OBJ)

# Volledige schoonmaak (inclusief uitvoerbaar bestand en backups)
cleanall: clean
	$(RM) $(BIN) *~

install:
	install -m 755 $(BIN) /usr/local/bin/

# Markeer targets als PHONY om conflicten te vermijden
.PHONY: all clean cleanall