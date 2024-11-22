#include "game.h"

int initGame(Game *game) {
    for (int i = 0; i < 12; i++) {
        game->board.cells[i].numSeeds = 4;
    }

    snprintf(game->player1.nickname, sizeof(game->player1.nickname), "%s", game->player1.nickname);
    snprintf(game->player2.nickname, sizeof(game->player2.nickname), "%s", game->player2.nickname);

    game->player1.score = 0;
    game->player2.score = 0;
    return 0;
}

int displayBoard(Game *game) {
    printf("\n  Player 1: %s, Score: %d\n", game->player1.nickname, game->player1.score);

    // Display the upper line (cells 0 to 5 for Player 1)
    printf("   ");
    for (int i = 0; i < 6; i++) {
        printf("%2d ", game->board.cells[i].numSeeds); // Display cells from left to right
    }
    printf("\n");

    // Display the lower line (cells 11 to 6 for Player 2)
    printf("   ");
    for (int i = 11; i >= 6; i--) {
        printf("%2d ", game->board.cells[i].numSeeds); // Display cells from right to left
    }
    printf("\n");

    printf("  Player 2: %s, Score: %d\n\n", game->player2.nickname, game->player2.score);
    printf("  ---------------------------------\n");
    return 0;
}

int remainingSeedsForPlayer(Game *game, int player) {
    int numSeeds = 0;
    for (int i = (player - 1) * 6; i < player * 6; i++) {
        numSeeds += game->board.cells[i].numSeeds;
    }
    return numSeeds;
}

int canFeedOpponent(Game *game, int starvingPlayer) {
    int opponent = (starvingPlayer == 1) ? 2 : 1;
    int opponentStart = (opponent - 1) * 6;
    int opponentEnd = opponent * 6;
    int starvingPlayerStart = (starvingPlayer - 1) * 6;
    int starvingPlayerEnd = starvingPlayer * 6;

    // Check each cell of the opponent to see if a move can feed the starving player
    for (int i = opponentStart; i < opponentEnd; i++) {
        int numSeeds = game->board.cells[i].numSeeds;
        if (numSeeds > 0) {
            // Simulate the distribution of seeds
            for (int j = 1; j <= numSeeds; j++) {
                int position = (i + j) % 12;
                // Check if a seed falls into the starving player's side
                if (position >= starvingPlayerStart && position < starvingPlayerEnd) {
                    return 1; // The opponent can feed the starving player
                }
            }
        }
    }
    return 0; // The opponent cannot feed the starving player
}

int moveFeedsOpponent(Game *game, int playedCell, int player) {
    int numSeeds = game->board.cells[playedCell].numSeeds;
    int index = playedCell;

    for (int i = 0; i < numSeeds; i++) {
        index = (index + 1) % 12;
    }

    int opponent = (player == 1) ? 2 : 1;
    if ((opponent == 1 && index < 6) || (opponent == 2 && index >= 6)) {
        return 1; // The move feeds the opponent
    }

    return 0; // The move does not feed the opponent
}

void captureSeeds(Game *game, int playedCell, int player) {
    int numSeedsCaptured = 0;
    Board tempBoard;
    memcpy(&tempBoard, &(game->board), sizeof(Board));

    int i = playedCell;
    while ((tempBoard.cells[i].numSeeds == 2 || tempBoard.cells[i].numSeeds == 3) &&
           ((player == 1 && i >= 6) || (player == 2 && i < 6))) {
        numSeedsCaptured += tempBoard.cells[i].numSeeds;
        tempBoard.cells[i].numSeeds = 0;
        i = (i + 11) % 12; // Move backward
    }

    if (numSeedsCaptured < remainingSeedsForPlayer(game, 3 - player)) {
        if (player == 1) game->player1.score += numSeedsCaptured;
        else game->player2.score += numSeedsCaptured;
        memcpy(&(game->board), &tempBoard, sizeof(Board));
    }
}

int playMoveGame(Game *game, int playedCell, int player) {
    int opponent = (player == 1) ? 2 : 1;

    // Check that the player plays in his own side and that the cell is not empty
    if (game->board.cells[playedCell].numSeeds == 0) {
        printf("Illegal move: the cell is empty.\n");
        return 1;
    }
    if ((player == 1 && (playedCell < 0 || playedCell > 6)) ||
        (player == 2 && (playedCell < 6 || playedCell >= 12))) {
        printf("Illegal move: the player plays outside his side.\n");
        return 1;
    }

    // If the opponent is starving, the move must feed him
    if (remainingSeedsForPlayer(game, opponent) == 0 && !moveFeedsOpponent(game, playedCell, player)) {
        printf("Illegal move: the opponent is starving, the move must feed him.\n");
        return 1;
    }

    int numSeeds = game->board.cells[playedCell].numSeeds;
    game->board.cells[playedCell].numSeeds = 0;

    int index = playedCell;
    for (int i = 0; i < numSeeds; i++) {
        index = (index + 1) % 12;
        game->board.cells[index].numSeeds++;
    }

    // Check that the capture is only made in the opponent's side
    if ((player == 1 && index >= 6) || (player == 2 && index < 6)) {
        captureSeeds(game, index, player);
    }

    return 0; // Valid move
}

int canForceCapture(Game *game, int player) {
    int playerStart = (player - 1) * 6;
    int playerEnd = player * 6;
    int opponentStart = (player == 1) ? 6 : 0;
    int opponentEnd = (player == 1) ? 12 : 6;

    // Go through all the player's cells
    for (int i = playerStart; i < playerEnd; i++) {
        int numSeeds = game->board.cells[i].numSeeds;
        if (numSeeds > 0) {
            // Simulate the distribution of seeds
            for (int j = 1; j <= numSeeds; j++) {
                int position = (i + j) % 12;
                // Check if the last seed falls into an opponent's cell containing 2 or 3 seeds
                if (position >= opponentStart && position < opponentEnd &&
                    j == numSeeds && // The last seed
                    (game->board.cells[position].numSeeds == 2 || game->board.cells[position].numSeeds == 3)) {
                    return 1; // The player can force a capture
                }
            }
        }
    }
    return 0; // No move allows to force a capture
}

int checkEndOfGame(Game *game) {
    // Check if a player has more than 24 points, which means they have won
    if (game->player1.score > 24 || game->player2.score > 24) {
        return 1;
    }

    // Check if one of the players is starving and if the opponent can feed them
    if (remainingSeedsForPlayer(game, 1) == 0) {
        if (!canFeedOpponent(game, 1)) {
            // Distribute the remaining seeds to the player who is playing
            for (int i = 6; i < 12; i++) {
                game->player2.score += game->board.cells[i].numSeeds;
                game->board.cells[i].numSeeds = 0;
            }
            return 1; // Player 1 is starving and cannot be fed
        }
    } else if (remainingSeedsForPlayer(game, 2) == 0) {
        if (!canFeedOpponent(game, 2)) {
            // Distribute the remaining seeds to the player who is playing
            for (int i = 0; i < 6; i++) {
                game->player1.score += game->board.cells[i].numSeeds;
                game->board.cells[i].numSeeds = 0;
            }
            return 1; // Player 2 is starving and cannot be fed
        }
    }

    // Check if no player can force a capture and if the total number of seeds is less than 3
    int totalSeeds = 0;
    for (int i = 0; i < 12; i++) totalSeeds += game->board.cells[i].numSeeds;
    if (totalSeeds<=3 && !canForceCapture(game, 1) && !canForceCapture(game, 2)) {
        // Each player get back the remaining seeds on their side
        for (int i = 0; i < 6; i++) {
            game->player1.score += game->board.cells[i].numSeeds;
            game->board.cells[i].numSeeds = 0;
        }
        for (int i = 6; i < 12; i++) {
            game->player2.score += game->board.cells[i].numSeeds;
            game->board.cells[i].numSeeds = 0;
        }
        return 1;
    }

    return 0; // The game can continue
}