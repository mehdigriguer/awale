CC = gcc
CFLAGS = -Wall -Wextra
SERVER_SOURCES = Game/game.c Server/server.c
CLIENT_SOURCES = Client/client.c
SERVER_EXEC = serverexec
CLIENT_EXEC = clientexec
USERS_FILE = Server/users.txt
PLAYERS_DIR = Server/players

all: $(SERVER_EXEC) $(CLIENT_EXEC) $(USERS_FILE)

# Create the executables
$(SERVER_EXEC): $(SERVER_SOURCES)
	$(CC) $(CFLAGS) -o $@ $^

$(CLIENT_EXEC): $(CLIENT_SOURCES)
	$(CC) $(CFLAGS) -o $@ $^

# Create the file users.txt with the specified users
$(USERS_FILE): | Server
	@echo "mehdi,m" > $@
	@echo "djalil,d" >> $@
	@echo "fred,f" >> $@
	@echo "felix,f" >> $@

# Ensure the Server directory exists
Server:
	@mkdir -p $@

$(PLAYERS_DIR):
	@mkdir -p $(PLAYERS_DIR)

clean:
	rm -f $(SERVER_EXEC) $(CLIENT_EXEC)
	@if [ -f $(USERS_FILE) ]; then \
		echo "Suppression de $(USERS_FILE)..."; \
		rm -f $(USERS_FILE); \
	fi
	@if [ -d $(PLAYERS_DIR) ]; then \
		echo "Suppression du dossier $(PLAYERS_DIR)..."; \
		rm -rf $(PLAYERS_DIR); \
	fi

.PHONY: all clean