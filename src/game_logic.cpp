
#include "game_logic.h"

//Methods to implement:
/*
    - Implement Blocking
    - Implement Challenges
    - 


*/



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
