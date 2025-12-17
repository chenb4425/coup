
#include "game_logic.h"

//Methods to implement:
/*
    - Implement Blocking
    - Implement Challenges
    - 


*/




void reset(GameState* game, const uint8_t num_players,                //init how many player we will ahve
                            const uint8_t (&deck)[DECK_SIZE]
                            const uint8_t (&money_init)[MAX_PLAYERS],                //mostly used for mock testing stuff 
                            const uint8_t (&c1)[MAX_PLAYERS],                        //Array for first card they have enforced as size MAX_PLAYERS
                            const uint8_t (&c1)[MAX_PLAYERS])                        //Array for second card they have enforced as size MAX_PLAYERS
                            {
    game->action_turn = 0;
    game->player_count = num_players;

    //copy all initial money down from python
    std::memcpy(game->money, money_init, MAX_PLAYERS * sizeof(uint8_t));

    //copy all cards given from python
    std::memcpy(game->player1_cards, c1, MAX_PLAYERS * sizeof(uint8_t));
    std::memcpy(game->player2_cards, c2, MAX_PLAYERS * sizeof(uint8_t));
    
}


//just does modulo on turns
void next_turn(GameState* game) {

}




///////////////////////////////////////////////////////////////
////////////////  MONEY METHODSSSS ////////////////////////////
////////////////////////////////////////////////////////////////
//All that happens is the turn changes, and the player gets 1 dolla
void income() {
    
    //whoevers action turn it is they just go,
    money[action_turn] += 1;
    action_turn++;
    challenge_turn++;
}


void foreign_aid() {

    if(!block()) {
        //whoevers action turn it is they just go,
        money[action_turn] += 2;
    }

    action_turn++;
    challenge_turn++;
    
}


void tax() {

    if(!challenge()) {
        //whoevers action turn it is they just go,
        money[action_turn] += 3;
    }

    action_turn++;
    challenge_turn++;

}
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////







/*
This method should essentially ask every player if they want to block or not
*/
bool block(Your_Turn_A action) {

    //we use action to tell the other player what action was just made,
    //then ask if they want to block

}





/*
Ask every player if they want to challenge or not
*/
bool challenge(Your_Turn_A action) {

    //we use action to tell the other player what action was just made,
    //then ask if they want to challenge

}
