#ifndef GAME_H
#define GAME_H

#include <stdio.h>
#include <string.h>

typedef struct {
    int numSeeds; // Number of seeds in a cell
} Cell;

typedef struct {
    Cell cells[12]; // Game board with 12 cells
} Board;

typedef struct {
    char nickname[20];
    int score;
} Player;

typedef struct {
    Board board;
    Player player1;
    Player player2;
    int currentTurn; // (1 or 2)
    int status;
} Game;

int initGame(Game *game);
int displayBoard(Game *game);
int remainingSeedsForPlayer(Game *game, int player);
int canFeedOpponent(Game *game, int starvingPlayer);
int moveFeedsOpponent(Game *game, int playedCell, int player);
void captureSeeds(Game *game, int playedCell, int player);
int playMoveGame(Game *game, int playedCell, int player);
int canForceCapture(Game *game, int player);
int checkEndOfGame(Game *game);

#endif