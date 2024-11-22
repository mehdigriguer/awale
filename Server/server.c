#include "server.h"

static void init(void)
{
#ifdef WIN32
    WSADATA wsa;
    int err = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (err < 0)
    {
        puts("WSAStartup failed!");
        exit(EXIT_FAILURE);
    }
#endif
}

static void end(void)
{
#ifdef WIN32
    WSACleanup();
#endif
}

Room rooms[MAX_ROOMS];
User users[MAX_CLIENTS];
int greatestIndexUsedbyUser = 0;
int greatestIndexUsedbyRoom = 0;

void initUsers(void)
{
    for (int i = 0; i < MAX_CLIENTS; i++) 
    {
        users[i].sock = INVALID_SOCKET;
        users[i].index = -1;
        users[i].isInGame = 0;
        users[i].isConnected = 0;
        users[i].indexOfRoom = -1;
        users[i].challenger = NULL;
        memset(users[i].username, 0, sizeof(users[i].username));
        memset(users[i].password, 0, sizeof(users[i].password));
    }
}
void initRooms(void)
{
    for (int i = 0; i < MAX_ROOMS; i++)
    {
        rooms[i].index = -1;
        rooms[i].player1 = NULL;
        rooms[i].player2 = NULL;
        rooms[i].status = 0;
    }
}
void createRoom(int indexPlayer1, int indexPlayer2)
{
    int freeIndex = -1;
    // find a free index in the rooms array
    for (int i = 0; i < MAX_ROOMS; i++){
        if (rooms[i].index == -1 && rooms[i].status == 0)
        {
            freeIndex = i;
            break;
        }
    }
    if (freeIndex == -1)
    {
        printf("Maximum number of rooms reached.\n");
        return;
    }

    User *player1 = findUserByIndex(indexPlayer1);
    User *player2 = findUserByIndex(indexPlayer2);
    Room *room = &rooms[freeIndex];
    room->index = freeIndex;

    printf("Room [%d] created: %s [%d] vs %s [%d] \n", room->index, player1->username, indexPlayer1, player2->username, indexPlayer2);

    room->player1 = player1;
    strcpy(room->game.player1.nickname, player1->username);
    room->player2 = player2;
    strcpy(room->game.player2.nickname, player2->username);

    srand(time(NULL)); // Déterminer aléatoirement quel joueur commence
    room->currentTurn = rand() % 2; // 0 pour player1, 1 pour player2
    room->status = 1;

    initGame(&room->game);

    player1->isInGame = 1;
    player2->isInGame = 1;
    player1->indexOfRoom = freeIndex;
    player2->indexOfRoom = freeIndex;

    greatestIndexUsedbyRoom++;

    displayBoardToUsers(player1, player2, &room->game);
    displayCurrentTurn(room->index);
}
void playMove(int roomIndex, User *player, int playedCell)
{
    Room *room = findRoomByIndex(roomIndex);
    if (!room || room->status != 1 || !player) return;
    Game *game = &room->game;
    int playerIndex = (player == room->player1) ? 0 : 1;

    if (room->currentTurn != playerIndex)
    {
        write_client(player->sock, "It's not your turn.\n");
        return;
    }

    if (playMoveGame(game, playedCell, playerIndex + 1) == 0)
    {
        displayBoardToUsers(room->player1, room->player2, game);
        room->currentTurn = 1 - room->currentTurn;
        displayCurrentTurn(room->index);
    }
    else write_client(player->sock, "Illegal move, try again.\n");

    if (checkEndOfGame(game)) endGame(room->index);
}
void endGame(int roomIndex)
{
    Room *room = findRoomByIndex(roomIndex);
    if (!room || room->status != 1) return;

    printf("Room [%d] closed: %s [%d] vs %s [%d]\n", room->index, room->player1->username, room->player1->index, room->player2->username, room->player2->index);

    room->player1->isInGame = 0;
    room->player2->isInGame = 0;
    room->player1->challenger = NULL;
    room->player2->challenger = NULL;
    room->player1->indexOfRoom = -1;
    room->player2->indexOfRoom = -1;
    if (room->player1->sock != INVALID_SOCKET) write_client(room->player1->sock, "The game is over.\n");
    if (room->player2->sock != INVALID_SOCKET) write_client(room->player2->sock, "The game is over.\n");

    room->status = 0; // put the room back to free
}

// handlers
void handleNewClient(SOCKET sock)
{
    int freeIndex = -1;
    // find a free index in the user array
    for (int i = 0; i < MAX_CLIENTS; i++){
        if (users[i].index == -1 && users[i].sock == INVALID_SOCKET)
        {
            freeIndex = i;
            break;
        }
    }
    if (freeIndex == -1)
    {
        printf("Maximum number of rooms reached.\n");
        closesocket(sock);
        return;
    }

    User *user = &users[freeIndex];
    user->index = freeIndex;
    user->sock = sock;
    printf("New client connected!\n");
    write_client(sock, "Welcome to the awale server !\n");
    displayHelp(user);
    greatestIndexUsedbyUser++;
}
void handleSignin(User *user, const char *username, const char *password)
{
    int result = saveUserInDatabase(username, password);
    if (result == 1)
    {
        char user_dir[150];
        snprintf(user_dir, sizeof(user_dir), "players/%s", username);
        mkdir(user_dir, 0777);

        write_client(user->sock, "Signup successful! Log in with /login.\n");
        printf("New user registered and directory created: '%s'\n", username);
    }
    else if (result == 0) write_client(user->sock, "Username already taken.\n");
    else write_client(user->sock, "Error during signup.\n");
}
void handleLogin(User *user, const char *username, const char *password)
{
    if (isAlreadyConnected(username)) write_client(user->sock, "Error: This user is already connected elsewhere.\n");
    else if (credentialsAreValid(username, password))
    {
        strncpy(user->username, username, sizeof(user->username) - 1); // Copy the username
        user->isConnected = 1; // Mark the user as connected
        char buffer[BUF_SIZE] = {0};
        snprintf(buffer, BUF_SIZE, "Welcome %s! \n", user->username);
        write_client(user->sock, buffer);
        displayHelp(user);
    }
    else write_client(user->sock, "Incorrect credentials.\n");
}
void handleLogout(int indexUser) 
{
    removeClient(indexUser, 1);
}
void handleMessage(User *user, char *message)
{
    if (!user->isConnected)
    {
        if (strncmp(message, "/login ", 7) == 0)
        {
            char username[50], password[50];
            if (sscanf(message + 7, "%49s %49s", username, password) == 2)
            {
                handleLogin(user, username, password);
            }
            else
            {
                write_client(user->sock, "Incorrect format. Use: /login [username] [password]\n");
            }
        }
        else if (strncmp(message, "/signin ", 8) == 0)
        {
            char username[50], password[50];
            if (sscanf(message + 8, "%49s %49s", username, password) == 2)
            {
                handleSignin(user, username, password);
            }
            else
            {
                write_client(user->sock, "Incorrect format. Use: /signin [username] [password]\n");
            }
        }
        else write_client(user->sock, "You must be logged in to use this command.\n");
    }
    else 
    {
        if (user->isInGame)
        {
            if (strncmp(message, "/play ", 6) == 0)
            {
                int playedCell = atoi(message + 6) - 1;
                printf("%d", user->indexOfRoom);
                Room *room = &rooms[user->indexOfRoom];
                printf("%d", room->index);
                playMove(room->index, user, playedCell);
            }
            else if (strcmp(message, "/logout") == 0)
            {
                handleLogout(user->index);
            }
            else if (strcmp(message, "/help") == 0)
            {
                displayHelp(user);
            }
            else broadcastMessage(user, message);
        }
        else {
            if (strncmp(message, "/challenge ", 11) == 0)
            {
                char *username = message + 11;
                User *opponent = findUserByUsername(username);
                if (opponent != NULL && !opponent->isInGame && opponent != user)
                {   
                    user->challenger = opponent;
                    opponent->challenger = user;
                    write_client(opponent->sock, "You have received a challenge! Type /accept to accept or /decline to decline.\n");
                    write_client(user->sock, "Challenge sent! Waiting for a response.\n");
                }
                else
                {
                    write_client(user->sock, "Player not available.\n");
                }
            }
            else if (strcmp(message, "/accept") == 0 && user->challenger != NULL)
                {
                    write_client(user->challenger->sock, "Challenge accepted.\n");
                    write_client(user->sock, "Challenge accepted.\n");
                    createRoom(user->index, user->challenger->index);
                }
            else if (strcmp(message, "/decline") == 0 && user->challenger != NULL)
                {
                    write_client(user->challenger->sock, "Challenge declined.\n");
                    write_client(user->sock, "Challenge declined.\n");
                    user->challenger->challenger = NULL;
                    user->challenger = NULL; 
                }
            else if (strcmp(message, "/list") == 0)
                {
                    char buffer[BUF_SIZE] = "Online players:\n";
                    for (int i = 0; i < greatestIndexUsedbyUser; i++)
                    {
                        if (users[i].isConnected && !users[i].isInGame)
                        {
                            strncat(buffer, users[i].username, BUF_SIZE - strlen(buffer) - 1);
                            strncat(buffer, "\n", BUF_SIZE - strlen(buffer) - 1);
                        }
                    }
                    write_client(user->sock, buffer);
                }
            else if (strcmp(message, "/logout") == 0)
                {
                    handleLogout(user->index);
                }
            else if (strcmp(message, "/help") == 0)
            {
                displayHelp(user);
            }
            else broadcastMessage(user, message);
        }
    }  
}

// communication
void displayBoardToUsers(User *player1, User *player2, Game *game)
{
    char buffer[BUF_SIZE] = {0}; // Initialize to zero

    // Add fixed lines
    strncat(buffer, "  ---------------------------------\n", BUF_SIZE - strlen(buffer) - 1);
    snprintf(buffer + strlen(buffer), BUF_SIZE - strlen(buffer),
             "  Player 1: %s, Score: %d\n",
             game->player1.nickname, game->player1.score);

    // Upper line (cells 1 to 6 for Player 1)
    strncat(buffer, "   ", BUF_SIZE - strlen(buffer) - 1);
    for (int i = 0; i < 6; i++)
    {
        char temp[10] = {0};
        snprintf(temp, sizeof(temp), "%2d ", game->board.cells[i].numSeeds);
        strncat(buffer, temp, BUF_SIZE - strlen(buffer) - 1);
    }
    strncat(buffer, "\n", BUF_SIZE - strlen(buffer) - 1);

    // Lower line (cells 12 to 7 for Player 2)
    strncat(buffer, "   ", BUF_SIZE - strlen(buffer) - 1);
    for (int i = 11; i >= 6; i--)
    {
        char temp[10] = {0};
        snprintf(temp, sizeof(temp), "%2d ", game->board.cells[i].numSeeds);
        strncat(buffer, temp, BUF_SIZE - strlen(buffer) - 1);
    }
    strncat(buffer, "\n", BUF_SIZE - strlen(buffer) - 1);

    // Add Player 2's information
    snprintf(buffer + strlen(buffer), BUF_SIZE - strlen(buffer),
             "  Player 2: %s, Score: %d\n",
             game->player2.nickname, game->player2.score);

    // Separation line
    strncat(buffer, "  ---------------------------------\n", BUF_SIZE - strlen(buffer) - 1);

    // Send the board to both players
    write_client(player1->sock, buffer);
    write_client(player2->sock, buffer);
}
void displayCurrentTurn(int roomIndex)
{
    Room *room = findRoomByIndex(roomIndex);
    if (room->currentTurn == 0)
    {
        char messagePlayer1[BUF_SIZE];
        char messagePlayer2[BUF_SIZE];

        snprintf(messagePlayer1, BUF_SIZE, "It's your turn, %s!\n", room->player1->username);
        snprintf(messagePlayer2, BUF_SIZE, "It's %s's turn.\n", room->player1->username);

        write_client(room->player1->sock, messagePlayer1);
        write_client(room->player2->sock, messagePlayer2);
    }
    else
    {
        char messagePlayer1[BUF_SIZE];
        char messagePlayer2[BUF_SIZE];

        snprintf(messagePlayer1, BUF_SIZE, "It's %s's turn.\n", room->player2->username);
        snprintf(messagePlayer2, BUF_SIZE, "It's your turn, %s!\n", room->player2->username);

        write_client(room->player1->sock, messagePlayer1);
        write_client(room->player2->sock, messagePlayer2);
    }
}
void displayHelp(User *user)
{
    if (!user->isConnected)
    {
        write_client(user->sock, "Here are the available commands:\n");
        write_client(user->sock, "/login <username> <password> - Log in\n");
        write_client(user->sock, "/signin <username> <password> - Sign up\n");
    }
    else
    {
        if (!user->isInGame)
        {
            write_client(user->sock, "Here are the available commands:\n");
            write_client(user->sock, "/challenge <name> - Challenge a player\n");
            write_client(user->sock, "/list - Display the list of online players\n");
            write_client(user->sock, "/logout - Log out\n");
            write_client(user->sock, "/help - Display help\n");
        }
        else
        {
            write_client(user->sock, "Here are the available commands:\n");
            write_client(user->sock, "/play <cell> - Play on a cell\n");
            write_client(user->sock, "/logout - Log out\n");
            write_client(user->sock, "/help - Display help\n");
        }
    }
}
void broadcastMessage(User *sender, const char *message)
{
    char buffer[BUF_SIZE];
    snprintf(buffer, BUF_SIZE, "%s: %s\n", sender->username, message);
    for (int i = 0; i < greatestIndexUsedbyUser; i++) write_client(users[i].sock, buffer);
}

// handle users
User *findUserByUsername(const char *username)
{
    for (int i = 0; i < greatestIndexUsedbyUser; i++)
    {
        if (strcmp(users[i].username, username) == 0) return &users[i];
    }
    return NULL;
}
User *findUserByIndex(int index)
{
    for (int i = 0; i < MAX_ROOMS; i++)
    {
        if (users[i].index == index && users[i].sock != INVALID_SOCKET) return &users[i];
    }
    return NULL;
}
Room *findRoomByIndex(int index)
{
    for (int i = 0; i < MAX_ROOMS; i++)
    {
        if (rooms[i].index == index && rooms[i].status == 1) return &rooms[i];
    }
    return NULL;
}
int isAlreadyConnected(const char *username)
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (users[i].isConnected && strcmp(users[i].username, username) == 0) return 1; // The user is already connected
    }
    return 0; // The user is not connected
}
int credentialsAreValid(const char *username, const char *password)
{
    FILE *file = fopen("Server/users.txt", "r");
    if (file == NULL)
    {
        perror("Error opening the file users.txt");
        return 0;
    }

    char line[150];
    char file_username[50], file_password[50];

    // Read each line of the file
    while (fgets(line, sizeof(line), file))
    {
        // Remove trailing newline
        line[strcspn(line, "\n")] = '\0';

        // Parse username and password from the line
        if (sscanf(line, "%49[^,],%49[^,]", file_username, file_password) == 2)
        {
            if (strcmp(file_username, username) == 0 && strcmp(file_password, password) == 0)
            {
                fclose(file);
                return 1; // Authentication successful
            }
        }
    }

    fclose(file);
    return 0; // Authentication failed
}
int saveUserInDatabase(const char *username, const char *password)
{
    // Check if the user already exists
    if (findUserByUsername(username) != NULL)
    {
        return 0; // User already exists
    }

    FILE *file = fopen("Server/users.txt", "a");
    if (!file)
    {
        perror("Error opening the file users.txt");
        return -1;
    }

    // Append the new user to the file
    fprintf(file, "%s,%s\n", username, password);
    fclose(file);

    // Create main 'players' directory if it doesn't exist
    char base_dir[] = "Server/players";
    struct stat st = {0};
    if (stat(base_dir, &st) == -1)
    {
        mkdir(base_dir, 0777);
    }

    // Create user's sub-directory
    char user_dir[150];
    snprintf(user_dir, sizeof(user_dir), "%s/%s", base_dir, username);
    if (mkdir(user_dir, 0777) != 0)
    {
        perror("Error creating user directory");
        return -1;
    }

    return 1; // Signup successful
}
static void removeClient(int indexUser, int hasLogout)
{
    User *user = findUserByIndex(indexUser);
    if (user->isConnected) 
    {
        if (user->isInGame) {
            Room *room = &rooms[user->indexOfRoom];
            if (room->status == 1) { // Si la salle est active
                User *opponent = user->challenger;
                if (opponent && opponent->sock != INVALID_SOCKET) write_client(opponent->sock, "You have won by resignation of your opponent.\n");
                if (user->sock != INVALID_SOCKET) write_client(user->sock, "You have resigned.\n");
            }
            endGame(room->index);
        }
        if (hasLogout == 1) {
            if (user->sock != INVALID_SOCKET) write_client(user->sock, "You logged out.\n");
            printf("User %s has logged out.\n", user->username);
        }
        else {
            if (user->sock != INVALID_SOCKET) write_client(user->sock, "You have been disconnected.\n");
            printf("User %s has been disconnected.\n", user->username);
        }
        user->isConnected = 0;
    }
    else 
    {
        if (user->sock != INVALID_SOCKET) write_client(user->sock, "You have been disconnected.\n");
        printf("A client has been disconnected.\n");
    }
    end_connection(user->sock);
    user->sock = INVALID_SOCKET;
}

// server logic
static void app(void)
{
    SOCKET sock = init_connection();
    fd_set rdfs;
    char buffer[BUF_SIZE];
    User *user;
    int max = sock;

    // Initialize structures
    initUsers();
    initRooms();

    // Main loop
    while (1)
    {
        // Prepare the set of sockets for `select`
        FD_ZERO(&rdfs);
        FD_SET(STDIN_FILENO, &rdfs);
        FD_SET(sock, &rdfs);

        // Add all connected clients to the set
        for (int i = 0; i < greatestIndexUsedbyUser; i++)
        {
            FD_SET(users[i].sock, &rdfs);
            max = (users[i].sock > max) ? users[i].sock : max;
        }

        // Wait for activity on one of the sockets
        if (select(max + 1, &rdfs, NULL, NULL, NULL) == -1)
        {
            if (errno == EINTR) continue; // Retry on interrupt
            perror("select()");
            break;
        }

        // Handle input from the terminal to stop the server
        if (FD_ISSET(STDIN_FILENO, &rdfs))
        {
            break;
        }

        // Handle new client connections
        if (FD_ISSET(sock, &rdfs))
        {
            SOCKADDR_IN csin = {0};
            socklen_t sinsize = sizeof csin;
            SOCKET csock = accept(sock, (SOCKADDR *)&csin, &sinsize);

            if (csock == SOCKET_ERROR)
            {
                perror("accept()");
                continue;
            }

            handleNewClient(csock);
        }

        // Handle messages from existing clients
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            user = &users[i];
            if (FD_ISSET(user->sock, &rdfs) && user->sock != INVALID_SOCKET)
            {
                int n = read_client(user->sock, buffer);
                if (n <= 0) // control c
                {
                    removeClient(i, 0);
                    continue;
                }
                if (user->sock != INVALID_SOCKET)
                {
                    buffer[n] = '\0'; // Null-terminate the buffer
                    handleMessage(user, buffer);
                }
                
            }
        }
    }

    // Close all remaining connections
    for (int i = 0; i < greatestIndexUsedbyUser; i++) end_connection(users[i].sock);
    end_connection(sock);
}
static int init_connection(void)
{
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    SOCKADDR_IN sin = {0};

    if (sock == INVALID_SOCKET)
    {
        perror("socket()");
        exit(errno);
    }

    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(PORT);
    sin.sin_family = AF_INET;

    if (bind(sock, (SOCKADDR *)&sin, sizeof sin) == SOCKET_ERROR)
    {
        perror("bind()");
        exit(errno);
    }

    if (listen(sock, MAX_CLIENTS) == SOCKET_ERROR)
    {
        perror("listen()");
        exit(errno);
    }

    return sock;
}
static void end_connection(int sock)
{
    if (closesocket(sock) == SOCKET_ERROR)
    {
        perror("closesocket()");
    }
    else
    {
        printf("Socket %d successfully closed.\n", sock);
    }
}
static int read_client(SOCKET sock, char *buffer)
{
    int n = recv(sock, buffer, BUF_SIZE - 1, 0);
    if (n < 0)
    {
        perror("recv()");
        n = 0;
    }

    buffer[n] = 0;

    return n;
}
static void write_client(SOCKET sock, const char *buffer)
{
    if (sock == INVALID_SOCKET) {
        fprintf(stderr, "Attempt to write to an invalid socket.\n");
        return;
    }
    
    if (send(sock, buffer, strlen(buffer), 0) < 0)
    {
        perror("send()");
        exit(errno);
    }
}

int main()
{
    init();
    app();
    end();

    return EXIT_SUCCESS;
}