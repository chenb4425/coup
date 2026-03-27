#pragma once
// ============================================================
// coup.hpp  –  Coup card-game state engine
// ============================================================
// Rules synopsis
// ──────────────
//  • 2-6 players, each starts with 2 hidden cards + 2 coins.
//  • On your turn pick one action; others may challenge/block.
//  • Lose both influence cards → eliminated. Last alive wins.
//
// State-machine phases
// ─────────────────────
//  ACTION_SELECTION         active player picks an action
//  AWAITING_ACTION_RESPONSE other players may PASS / CHALLENGE / BLOCK
//  AWAITING_BLOCK_RESPONSE  any player may PASS / CHALLENGE the block
//  EXCHANGE_PICKING         Ambassador: actor picks cards to keep one at a time
//  EXCHANGE_RETURNING       Actor picks the order the unchosen cards return to deck
//  LOSE_INFLUENCE           designated player picks which card to reveal
//  GAME_OVER
//
// Exchange detail
// ────────────────
//  Actor sees exchCards_ = [own live card(s) | 2 drawn].
//  Phase 1 – EXCHANGE_PICKING:   submit EXCHANGE_KEEP(i) for each card to keep.
//  Phase 2 – EXCHANGE_RETURNING: submit EXCHANGE_RETURN(i) to set return order;
//                                 the very last unchosen card is placed back
//                                 automatically (no extra move needed).
//
// legalMovesToIntegers() encoding
// ─────────────────────────────────
//  Each legal move maps to a unique 3-D integer point (action, target, aux):
//    action  = ActionType cast to int
//    target  = player index, or numPlayers() for untargeted moves
//    aux     = card int for BLOCK; slot for LOSE_CARD;
//              exchCards_ index for EXCHANGE_KEEP / EXCHANGE_RETURN; 0 otherwise
// ============================================================

#include <array>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace coup {

// ─── Enumerations ────────────────────────────────────────────────────────────

enum class Card : uint8_t {
    DUKE       = 0,
    ASSASSIN   = 1,
    CAPTAIN    = 2,
    AMBASSADOR = 3,
    CONTESSA   = 4,
    NONE       = 255  // sentinel / unknown
};

/// Every move ever submitted to applyMove() is one of these.
enum class ActionType : uint8_t {
    // ── Primary actions (ACTION_SELECTION phase) ──────────────
    INCOME      = 0,  ///< Always: +1 coin
    FOREIGN_AID = 1,  ///< Always: +2 coins  (blockable by Duke)
    COUP        = 2,  ///< Always: -7 coins, target loses influence (mandatory at 10+ coins)
    TAX         = 3,  ///< Duke:       +3 coins
    ASSASSINATE = 4,  ///< Assassin:   -3 coins, target loses influence (blockable by Contessa)
    STEAL       = 5,  ///< Captain:    +2 coins from target (blockable by Captain / Ambassador)
    EXCHANGE    = 6,  ///< Ambassador: swap cards with the court deck
    // ── Response actions ──────────────────────────────────────
    PASS        = 7,  ///< Accept / do nothing
    CHALLENGE   = 8,  ///< Call the current claim a bluff
    BLOCK       = 9,  ///< Block the current action (specify which card you claim)
    // ── Resolution actions ────────────────────────────────────
    LOSE_CARD      = 10, ///< Reveal one of your cards (LOSE_INFLUENCE phase); cardIdx = slot 0/1
    KEEP_CARDS     = 11, ///< Legacy bitmask keep – no longer generated; use EXCHANGE_KEEP instead
    // ── Exchange step-by-step actions ─────────────────────────
    EXCHANGE_KEEP   = 12, ///< EXCHANGE_PICKING phase: keep exchCards_[cardIdx]
    EXCHANGE_RETURN = 13, ///< EXCHANGE_RETURNING phase: return exchCards_[cardIdx] next
};

enum class Phase : uint8_t {
    ACTION_SELECTION,
    AWAITING_ACTION_RESPONSE,
    AWAITING_BLOCK_RESPONSE,
    EXCHANGE_SELECTION,  ///< Legacy – no longer entered; kept for ABI stability
    LOSE_INFLUENCE,
    GAME_OVER,
    EXCHANGE_PICKING,    ///< Actor picks cards to keep one at a time (EXCHANGE_KEEP)
    EXCHANGE_RETURNING,  ///< Actor picks return order one at a time (EXCHANGE_RETURN)
};

/// Explains why a player is currently being asked to lose influence.
enum class LoseReason : uint8_t {
    COUP_TARGET,              ///< You were coup'd
    ASSASSINATE_TARGET,       ///< You were assassinated
    PROVED_ACTION,            ///< You challenged actor's card but they had it → you lose
    FAILED_CHALLENGE_ACTION,  ///< You claimed a card for an action but didn't have it → you lose
    PROVED_BLOCK,             ///< You challenged blocker's card but they had it → you lose
    FAILED_CHALLENGE_BLOCK,   ///< You claimed a blocking card but didn't have it → you lose
};

// ─── Data Structures ─────────────────────────────────────────────────────────

struct InfluenceCard {
    Card type  = Card::NONE;
    bool alive = true;  ///< alive=true  →  face-down, influence intact
                        ///< alive=false →  face-up,   influence lost
};

struct Player {
    std::string name;
    int         coins = 2;
    InfluenceCard cards[2];

    bool isEliminated() const { return !cards[0].alive && !cards[1].alive; }
    int  influence()    const { return (int)cards[0].alive + (int)cards[1].alive; }

    bool hasCard(Card c) const {
        return (cards[0].alive && cards[0].type == c) ||
               (cards[1].alive && cards[1].type == c);
    }
};

/// A move submitted to GameState::applyMove().
struct Move {
    ActionType action  = ActionType::PASS;
    int        target  = -1;          ///< Player index for COUP / ASSASSINATE / STEAL
    Card       card    = Card::NONE;  ///< Card claimed for character actions or BLOCK
    int        cardIdx = -1;          ///< Slot (0/1) for LOSE_CARD;
                                      ///< exchCards_ index for EXCHANGE_KEEP;
                                      ///< 0 or 1 for EXCHANGE_RETURN (which goes on top)

    // ── Convenience factories ──────────────────────────────────
    static Move income()                 { return {ActionType::INCOME}; }
    static Move foreignAid()             { return {ActionType::FOREIGN_AID}; }
    static Move coup(int tgt)            { return {ActionType::COUP,  tgt}; }
    static Move tax()                    { return {ActionType::TAX,   -1, Card::DUKE}; }
    static Move assassinate(int tgt)     { return {ActionType::ASSASSINATE, tgt, Card::ASSASSIN}; }
    static Move steal(int tgt)           { return {ActionType::STEAL, tgt, Card::CAPTAIN}; }
    static Move exchange()               { return {ActionType::EXCHANGE, -1, Card::AMBASSADOR}; }
    static Move pass()                   { return {ActionType::PASS}; }
    static Move challenge()              { return {ActionType::CHALLENGE}; }
    static Move block(Card claimed)      { return {ActionType::BLOCK, -1, claimed}; }
    static Move loseCard(int idx)        { return {ActionType::LOSE_CARD,   -1, Card::NONE, idx}; }
    static Move keepCards(int mask)      { return {ActionType::KEEP_CARDS,  -1, Card::NONE, mask}; } ///< legacy
    static Move exchangeKeep(int idx)    { return {ActionType::EXCHANGE_KEEP,   -1, Card::NONE, idx}; }
    static Move exchangeReturn(int idx)  { return {ActionType::EXCHANGE_RETURN, -1, Card::NONE, idx}; }
};

struct PendingAction {
    ActionType action       = ActionType::PASS;
    int        actor        = -1;
    int        target       = -1;
    Card       claimedCard  = Card::NONE;
    bool       challengeable = false;
    bool       blockable     = false;
};

struct PendingBlock {
    int  blocker     = -1;
    Card claimedCard = Card::NONE;
};

// ─── Explicit State Setup ─────────────────────────────────────────────────────

/// Describes one player's starting hand for use with GameState::fromSetup().
struct PlayerSetup {
    std::string   name;
    int           coins = 2;
    InfluenceCard card0 = { Card::NONE, true };   ///< {type, alive}
    InfluenceCard card1 = { Card::NONE, true };

    // ── Convenience factories ─────────────────────────────────
    /// Both cards face-down (full influence).
    static PlayerSetup make(std::string n, Card c0, Card c1, int coins = 2) {
        return { std::move(n), coins, {c0, true}, {c1, true} };
    }
    /// One live card, one already-revealed card (1 influence remaining).
    static PlayerSetup oneDown(std::string n, Card live, Card dead, int coins = 2) {
        return { std::move(n), coins, {live, true}, {dead, false} };
    }
    /// Both cards already revealed – player enters pre-eliminated.
    static PlayerSetup eliminated(std::string n, Card c0, Card c1) {
        return { std::move(n), 0, {c0, false}, {c1, false} };
    }
};

// ─── Game State ──────────────────────────────────────────────────────────────

class GameState {
public:
    /// Construct a fresh game.  seed controls deck shuffle + card replacement RNG.
    explicit GameState(const std::vector<std::string>& playerNames, uint64_t seed = 42);

    /// Build a game from an explicit per-player hand description.
    ///
    /// Cards not assigned to any player are placed into the shuffled court deck.
    /// Throws std::invalid_argument if player count is out of [2,6], any card
    /// slot is Card::NONE, or the card distribution exceeds the 15-card deck
    /// (3 copies each of the 5 characters).
    static GameState fromSetup(const std::vector<PlayerSetup>& setup,
                               int      activePlayerIndex = 0,
                               uint64_t seed             = 42);

    // ── Primary interface ─────────────────────────────────────
    /// Submit a move for the current respondingPlayer() (or activePlayer() in
    /// ACTION_SELECTION / EXCHANGE_PICKING / EXCHANGE_RETURNING / LOSE_INFLUENCE).
    void              applyMove(const Move& m);

    /// All legal moves available right now for the current responding / active player.
    std::vector<Move> legalMoves() const;

    /// Map every current legal move to a unique 3-D integer point.
    ///
    /// Coordinate axes:
    ///   x = (int)ActionType           – identifies the move category
    ///   y = target player index,      – or exchCards_ index / card slot;
    ///       numPlayers() when unused    numPlayers() when unused
    ///   z = (int)Card,                – card claimed / blocked with;
    ///       numPlayers() when unused    numPlayers() when unused
    ///
    /// Returned vector is in the same order as legalMoves() so the two can be
    /// zipped together: points[i] is the 3-D encoding of moves[i].
    std::vector<std::array<int,3>> legalMovesToIntegers() const;

    /// Convert any Move to its 3-D integer point (same encoding as above).
    static std::array<int,3> moveToPoint3D(const Move& m, int numPlayers);

    // ── State accessors ───────────────────────────────────────
    Phase       phase()              const { return phase_; }

    /// Whose overall turn it is (ACTION_SELECTION anchor).
    int         activePlayer()       const { return activePlayer_; }

    /// Who we are currently waiting on for a response (-1 = no pending responder).
    int         respondingPlayer()   const { return respondingPlayer_; }

    /// Player index who must next choose which card to lose (-1 if not applicable).
    int         loseInfluencePlayer() const { return loseInfluencePid_; }
    LoseReason  loseReason()          const { return loseReason_; }

    /// -1 while the game is ongoing; winning player index when GAME_OVER.
    int         winner()             const;
    bool        isTerminal()         const { return phase_ == Phase::GAME_OVER; }

    const std::vector<Player>& players()  const { return players_; }
    const Player& player(int i)           const { return players_.at(i); }
    int           numPlayers()            const { return (int)players_.size(); }

    const PendingAction&               pendingAction() const { return pending_; }
    const std::optional<PendingBlock>& pendingBlock()  const { return block_; }

    /// Full pool of cards visible during an exchange (actor's live cards + 2 drawn).
    /// Stays constant throughout all exchange sub-steps.
    const std::vector<Card>& exchangeCards() const { return exchCards_; }

    /// Which sub-step of exchange we are on:
    ///   0 = first EXCHANGE_KEEP pick
    ///   1 = second EXCHANGE_KEEP pick  (only when actor has 2 influence)
    ///   2 = EXCHANGE_RETURN ordering
    int exchangeStep() const { return exchStep_; }

    /// Indices into exchangeCards() that have been selected to keep so far.
    const std::vector<int>& exchangeKeptIndices() const { return exchKept_; }

    /// Cards in exchangeCards() not yet claimed:
    ///   steps 0/1 → still available to keep
    ///   step  2   → the two cards about to go back into the deck
    std::vector<Card> exchangeAvailableCards() const;

    /// Human-readable summary of the current state (reveals all cards for debugging).
    std::string toString() const;

    // ── String helpers (static) ───────────────────────────────
    static const char* cardName(Card c);
    static const char* actionName(ActionType a);
    static const char* phaseName(Phase p);
    static const char* loseReasonName(LoseReason r);

private:
    // ── Internal helpers ──────────────────────────────────────
    void initDeck();
    Card drawCard();
    int  nextAlive(int from) const;
    void advanceTurn();
    void beginActionResponsePhase();
    void advanceRespondingPlayer();

    // Per-phase apply helpers
    void applyActionSelection(const Move& m);
    void applyActionResponse(const Move& m);
    void applyBlockResponse(const Move& m);
    void applyExchangePicking(const Move& m);
    void applyExchangeReturning(const Move& m);
    void applyLoseInfluence(const Move& m);

    /// Commit kept cards to actor, push returned cards onto deck in chosen
    /// order (topChoice = which non-kept card index goes on top), advance turn.
    void finishExchange(int topChoice);

    void resolveAction();
    void actionFailed();
    void proveCard(int pid, Card c);
    void enterLoseInfluence(int pid, LoseReason reason, bool resolveAfter = false);
    void afterLoseInfluence();
    std::vector<Card> blockingCards() const;

    /// Indices into exchCards_ NOT in exchKept_ (cards still available / to return).
    std::vector<int> exchangeReturnIndices() const;

    // ── State ─────────────────────────────────────────────────
    std::vector<Player>         players_;
    std::vector<Card>           deck_;
    Phase                       phase_            = Phase::ACTION_SELECTION;
    int                         activePlayer_     = 0;
    int                         respondingPlayer_ = -1;
    std::vector<bool>           hasResponded_;

    PendingAction               pending_;
    std::optional<PendingBlock> block_;

    // Exchange sub-step state
    std::vector<Card>           exchCards_;   ///< full pool (live cards + drawn); constant during exchange
    int                         exchStep_ = 0;///< 0=first pick, 1=second pick, 2=order returns
    std::vector<int>            exchKept_;    ///< exchCards_ indices chosen to keep

    int                         loseInfluencePid_ = -1;
    LoseReason                  loseReason_       = LoseReason::COUP_TARGET;
    bool                        resolveAfterLose_ = false;

    std::mt19937                rng_;
};

} // namespace coup
