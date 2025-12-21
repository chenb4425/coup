
#include "game_logic.h"






void reset(GameState* game, const uint8_t num_players,                //init how many player we will ahve
                            const uint8_t (&deck_init)[DECK_SIZE]
                            const uint8_t (&money_init)[PLAYERS],                //mostly used for mock testing stuff 
                            const uint8_t (&c1)[PLAYERS],                        //Array for first card they have enforced as size MAX_PLAYERS
                            const uint8_t (&c1)[PLAYERS])                        //Array for second card they have enforced as size MAX_PLAYERS
                            {
    game->action_turn = 0;
    game->player_count = num_players;

    //Action stuff
    game->is_action_pending = false;

    game->action_name = 0;
    game->action_from = 0;
    game->action_to = 0;


    //Block stuff


    //init discard pile
    std::memset(game->discard_pile, 0, NUM_CARDS * sizeof(uint8_t));

    //init the deck
    std::memcpy(game->deck, deck_init, DECK_SIZE * sizeof(uint8_t));

    //init money
    std::memcpy(game->money, money_init, PLAYERS * sizeof(uint8_t));

    //init cards
    std::memcpy(game->player1_cards, c1, PLAYERS * sizeof(uint8_t));
    std::memcpy(game->player2_cards, c2, PLAYERS * sizeof(uint8_t));
    
}



// Action declaration
bool declare_action(GameState* game, TurnAction action, uint8_t target) {

    //Some action result in a blocking phase aswell as a challenge phase

    if(game->phase == BLOCK_WINDOW) {

    }


    if(game->phase == CHALLENGE_WINDOW)




    switch (action) {

        case INCOME


    }

}


// Actions
void income(GameState* game);
void foreign_aid(GameState* game);
void tax(GameState* game);
void exchange_cards(GameState* game);
void assasinate(GameState* game, uint8_t player);
void coup(GameState* game, uint8_t player);
void steal(GameState* game, uint8_t player);


// Responses
bool declare_block(GameState* game, CharacterCA block);
bool declare_challenge(GameState* game);

// Resolution
bool resolve(GameState* game);

// Tells user all legal actions available
uint8_t* legal_action_mask(const GameState* game) {

    




    //we have to ensure first that nothing has happened yet implies
    //game->phase = TURN_START
    if(game->turn_start == true) {

        uint8_t current_coins = game->coins[game->current_player];
        Turn_Action* actions;

        //if player has 7 coins or more they can coup
        if(current_coins >= 10) {
            actions = new Turn_Action[1]{COUP};
        }else if(current_coins >= 7) {
            actions = new Turn_Action[7]{INCOME,
                                        FOREIGN_AID,
                                        COUP,
                                        TAX,
                                        STEAL,
                                        EXCHANGE,
                                        ASSASSINATE};

        }else if(current_coins >= 3) {
            actions = new Turn_Action[6]{INCOME,
                                        FOREIGN_AID,
                                        TAX,
                                        STEAL,
                                        EXCHANGE,
                                        ASSASSINATE};

        }else {
            actions = new Turn_Action[6]{INCOME,
                                        FOREIGN_AID,
                                        TAX,
                                        STEAL,
                                        EXCHANGE};
        }

        //hopefully we can delete this down the line somehow
        return actions
    }


    if(game->block_window == true && game->challenge_window == true) {
        //people can only do block actions or challenge actions



    }else if(game->block_window == true) {
        //people can only 



    }else if(game->challenge_window == true) {

    }



}

// Utilities
uint8_t draw_card(GameState* game);
bool is_terminal(const GameState* game);