#ifndef GAME_LOGIC_H
#define GAME_LOGIC_H

#include <cstdint>


/*
TURN BASED GAME

#these will appear every turn
A. Any Turn Counter Actions
0 : don't challenge other opponent
1 : challenge other opponent
*/
//CA -> Counter Actions
enum Any_Turn_CA : uint8_t {
    NO_CHALLENGE,
    CHALLENGE
};


/*
B. Your turn Actions
0 : Income take 1
1 : Foreign aid take 2
2 : Coup someone use 7 coins
3 : Tax ie take 3 coins from treasury
4 : Steal 2 coins from another player
5 : Exchange 2 coins from court
6 : Assasinate with 3 coins
*/
//A -> Actions
enum Your_Turn_A : uint8_t {
    INCOME,
    FOREIGN,
    COUP,
    TAX,
    STEAL,
    EXCHANGE,
    ASSASINATE
};


/*
C. Character Counter Actions 
0 : block stealing (when someone steals from you special circumstance)
1 : Contessa someones assasination, only open if youve been asssasinated (Special circumstances)
2 : block foreign aid (when someone does foreign aid)
*/
//CA -> Counter Actions
enum Character_CA : uint8_t {
    BLOCK_STEAL,
    BLOCK_FOREIGN,
    BLOCK_ASSASIN
}




/*
We want to maximize the amount of game steps per second,
We will be calling this C++ game from python which implies
    - Only pure function calls
    - raw data in / raw data out

We will probably use something like Cython or C API or maybe Pybind11


So essentially C++ will handle
    - Rules
    - Transitions
    - Validity
    - Terminal detection

*/





// Python only has partial information
/*Python needs to be able to do a few things which include
    - It needs to shuffle deck
    - It needs to deal cards to different ml models
    - It needs to Choose action for each model described above
    - It needs to get 

*/

/*In python this is how we will train our models
GameState → Player1 (Model1) → action1
          Player2 (Model2) → action2
          …
C++ step → new GameState



*/



// C++ has all possible information
/*C++ needs to be able to do
    - It needs to receive cards
    - It needs to apply action to gamestate (give player money, player looses money, player loose card etc)
    - 


*/





//just set this 
const int  MAX_PLAYERS = 8;


//This will be our game, anytime anything new happens we just change this
struct GameState {


    //Players and their cards,
    //0 implies no card
    //1-5 implies 
    // 1 : duke
    // 2 : captain
    // 3 : Ambassador
    // 4 : Assasin
    // 5 : Contessa
    uint8_t card_1[MAX_PLAYERS];
    uint8_t card_2[MAX_PLAYERS];


    //amount of money each player gets
    uint8_t money[MAX_PLAYERS] = {0};




    //makeshift stack just push stuff then pop
    //every turn we iterate through the stack
    uint8_t stack[MAX_PLAYERS];



    //Whos turn it is at any give time
    uint8_t action_turn;

    //whos turn is it to challenge
    uint8_t challenge_turn;
    

    uint8_t player_count;


    //we only need player count to start the game
    GameState(uint8_t num_players) : player_count(num_players), action_turn(0), challenge_turn(1) {} 
};




//Apparently constructors are slow, and we want to reset the game state every iteration for training so -->
void reset(GameState* s, uint8_t num_players);





//Methods to implement:
/*
    - Implement Blocking
    - Implement Challenges
    - 


*/






#endif