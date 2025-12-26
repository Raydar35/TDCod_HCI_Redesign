#include "Game.h"
#include "Scene/Cutscene.h" // Corrected include path

int main() {
    Game game;
    TDCod::Cutscene cutscene; // Create Cutscene object

    // Run the cutscene before the game
    cutscene.run(game.getWindow());

    game.run();
    return 0;
}
