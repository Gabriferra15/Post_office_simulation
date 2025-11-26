CC = gcc
# -Iinclude dice al compilatore di cercare i file .h nella cartella 'include'
CFLAGS = -Wvla -Wextra -Werror -D_GNU_SOURCE -pthread -Iinclude

# Cartelle
SRC_DIR = src
BIN_DIR = bin
INC_DIR = include

# Target finale: compila tutto
all: directories direttore erogatore utente operatore

# Crea la cartella bin se non esiste
directories:
	mkdir -p $(BIN_DIR)

# --- REGOLE DI COMPILAZIONE ---

# Direttore
direttore: $(SRC_DIR)/main.c $(INC_DIR)/common.h
	$(CC) $(CFLAGS) -o $(BIN_DIR)/direttore $(SRC_DIR)/main.c

# Erogatore
erogatore: $(SRC_DIR)/erogatore.c $(INC_DIR)/common.h
	$(CC) $(CFLAGS) -o $(BIN_DIR)/erogatore $(SRC_DIR)/erogatore.c

# Utente
utente: $(SRC_DIR)/utente.c $(INC_DIR)/common.h
	$(CC) $(CFLAGS) -o $(BIN_DIR)/utente $(SRC_DIR)/utente.c

# Operatore
operatore: $(SRC_DIR)/operatore.c $(INC_DIR)/common.h
	$(CC) $(CFLAGS) -o $(BIN_DIR)/operatore $(SRC_DIR)/operatore.c

# Pulizia (rimuove la cartella bin)
clean:
	rm -rf $(BIN_DIR)