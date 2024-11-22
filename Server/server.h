#ifndef SERVER_H
#define SERVER_H

#ifdef WIN32

#include <winsock2.h>
#include <sys/select.h>

#elif defined(linux) || defined(__APPLE__)

#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h> /* close */
#include <netdb.h>  /* gethostbyname */
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket(s) close(s)
typedef int SOCKET;
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr SOCKADDR;
typedef struct in_addr IN_ADDR;

#else

#error Platform not supported

#endif

#define PORT      1977
#define BUF_SIZE  1024

#include "client.h"
#include "../Game/game.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#define MAX_CLIENTS 100
#define MAX_ROOMS   50

// Forward declaration to avoid circular dependencies between User and Room
typedef struct User User;
typedef struct Room Room;

struct User {
    SOCKET sock;
    char username[50];
    char password[50];
    int isConnected;     // 1 if connected, 0 otherwise
    int isInGame;        // 1 if in game, 0 otherwise
    int indexOfRoom;     // Index of the room in which the player is playing
    User *challenger;    // Pointer to the challenger
    int index;           // Index of the user in the users array
};

struct Room {
    User *player1;
    User *player2;
    Game game;
    int currentTurn; // 0 for player1, 1 for player2
    int status;      // 0 free, 1 occupied
    int index;      // Index of the room in the rooms array
};

// Global variables
extern Room rooms[MAX_ROOMS];
extern User users[MAX_CLIENTS];
extern int greatestIndexUsedbyUser;
extern int greatestIndexUsedbyRoom;

void initUsers(void);
void initRooms(void);
void createRoom(int indexPlayer1, int indexPlayer2);
void playMove(int roomIndex, User *player, int playedCell);
void endGame(int roomIndex);

// handlers
void handleNewClient(SOCKET sock);
void handleSignin(User *user, const char *username, const char *password);
void handleLogin(User *user, const char *username, const char *password);
void handleLogout(int indexUser);
void handleMessage(User *user, char *message);

// communication
void displayBoardToUsers(User *player1, User *player2, Game *game);
void displayCurrentTurn(int roomIndex);
void displayHelp(User *user);
void broadcastMessage(User *sender, const char *message);

// handle users
User *findUserByUsername(const char *username);
User *findUserByIndex(int index);
Room *findRoomByIndex(int index);
int isAlreadyConnected(const char *username);
int credentialsAreValid(const char *username, const char *password);
int saveUserInDatabase(const char *username, const char *password);
static void removeClient(int indexUser, int hasLogout);

// server logic
static void init(void);
static void end(void);
static void app(void);
static int init_connection(void);
static void end_connection(int sock);
static int read_client(SOCKET sock, char *buffer);
static void write_client(SOCKET sock, const char *buffer);

#endif