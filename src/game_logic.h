#ifndef GAME_LOGIC_H
#define GAME_LOGIC_H

#include <cstdint>


/*
An example would be if we had 3 players, and player 1 does tax
Player_1 options --> (ALL TURNACTIONS)
declare(game, TAX, target=0)
Player_2 options --> (Challenge Player_1, NONE)
declare(game, NONE, target=0) --> increment to next potential challenger
Player_3 options --> (Challenge Player_1, NONE)
declare_challenge(game); --> Looks at "game->pending_action_player"  and looks at his cards, if one of them corresponds to the action then good else he chooses which one to discard


After a block occurs it must be immediatedly resolved, and then people can continue to block
After a challenge occurs it must be immediatedly resolved then turn goes back to last user

*/






constexpr uint8_t NUM_CARDS = 15;
constexpr uint8_t PLAYERS = 5;
constexpr uint8_t CARDS_PER_PLAYER = 2;
constexpr uint8_t DECK_SIZE = NUM_CARDS - PLAYERS * CARDS_PER_PLAYER;

/* ===========================
   ENUMS
   =========================== */

// Main turn actions
enum class Turn_Action : uint8_t {
    NONE = 0,
    INCOME,
    FOREIGN_AID,
    COUP,
    TAX,
    STEAL,
    EXCHANGE,
    ASSASSINATE
};

// Character-based blocks
enum class Counter_Actions : uint8_t {
    NONE = 0,
    CHALLENGE,
    BLOCK_STEAL,
    BLOCK_FOREIGN_AID,
    BLOCK_ASSASSINATE
};

// // Game phase (VERY IMPORTANT)
// enum class Phase : uint8_t {
//     TURN_START,
//     ACTION_DECLARED,
//     BLOCK_WINDOW,
//     CHALLENGE_WINDOW,
//     RESOLVE_ACTION,
//     GAME_OVER
// };

/* ===========================
   GAME STATE
   =========================== */

struct GameState {

    // Deck & discard
    uint8_t deck[DECK_SIZE];
    uint8_t discard[NUM_CARDS];
    uint8_t deck_size;
    uint8_t discard_size;

    // Player state
    uint8_t card_1[PLAYERS];
    uint8_t card_2[PLAYERS];
    uint8_t coins[PLAYERS];
    bool alive[PLAYERS];

    // Turn info
    uint8_t current_player;
    uint8_t player_count;
    bool Turn_Start;
    bool Block_Window;
    bool Challenge_Window;
    bool Game_Over;

    // Pending action info
    Turn_Action pending_action;
    uint8_t pending_action_player;
    uint8_t action_target;        // player id, or 255 if none
    uint8_t blocking_player;      // who blocked (if any)
    uint8_t challenging_player;   // who challenged (if any)

};



/* ===========================
   CORE API (PURE FUNCTIONS)
   =========================== */

// Setup
void reset(GameState* game, const uint8_t (&deck_init)[DECK_SIZE],
                            const uint8_t (&money_init)[PLAYERS],    
                            const uint8_t (&c1)[PLAYERS], 
                            const uint8_t (&c1)[PLAYERS]);

                           
// Action declaration
bool declare_action(GameState* game, Turn_Action action, uint8_t target);

// Actions
void income(GameState* game);
void foreign_aid(GameState* game);
void tax(GameState* game);
void exchange_cards(GameState* game);
void assasinate(GameState* game, uint8_t player);
void coup(GameState* game, uint8_t player);
void steal(GameState* game, uint8_t player);


// Responses
bool declare_block(GameState* game, Character_CA block);
bool declare_challenge(GameState* game);

// Resolution
bool resolve(GameState* game);

// Tells user all legal actions available
uint8_t* legal_action_mask(const GameState* g);

// Utilities
uint8_t draw_card(GameState* game);
bool is_terminal(const GameState* game);

// Getters (Python-safe)
inline uint8_t get_current_player(const GameState* game) { return game->current_player; }
inline const uint8_t* get_coins(const GameState* game) { return game->coins; }
inline const uint8_t* get_cards1(const GameState* game) { return game->card_1; }
inline const uint8_t* get_cards2(const GameState* game) { return game->card_2; }

#endif // GAME_LOGIC_H
