

/*
TURN BASED GAME

#these will appear every turn
A. Any Turn Counter Actions
0 : don't challenge other opponent
1 : challenge other opponent



B. Your turn Actions
0 : Income take 1
1 : Foreign aid take 2
2 : Coup someone use 7 coins
3 : Tax ie take 3 coins from treasury
4 : Steal 2 coins from another player
5 : Exchange 2 coins from court
6 : Assasinate with 3 coins


C. Character Counter Actions 
0 : block stealing (when someone steals from you special circumstance)
1 : Contessa someones assasination, only open if youve been asssasinated (Special circumstances)
2 : block foreign aid (when someone does foreign aid)


*/




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



//0 implies no card
//1-5 implies 
// 1 : duke
// 2 : captain
// 3 : Ambassador
// 4 : Assasin
// 5 : Contessa
using Card = uint8_t;



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









//
struct GameState {

    



};
