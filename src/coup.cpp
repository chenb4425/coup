// ============================================================
// coup.cpp  –  Coup card-game state engine implementation
// ============================================================
#include "coup.hpp"

#include <algorithm>
#include <cassert>
#include <sstream>
#include <stdexcept>

namespace coup {

// ─── Static string helpers ───────────────────────────────────────────────────

const char* GameState::cardName(Card c) {
    switch (c) {
        case Card::DUKE:       return "Duke";
        case Card::ASSASSIN:   return "Assassin";
        case Card::CAPTAIN:    return "Captain";
        case Card::AMBASSADOR: return "Ambassador";
        case Card::CONTESSA:   return "Contessa";
        default:               return "None";
    }
}

const char* GameState::actionName(ActionType a) {
    switch (a) {
        case ActionType::INCOME:      return "Income";
        case ActionType::FOREIGN_AID: return "ForeignAid";
        case ActionType::COUP:        return "Coup";
        case ActionType::TAX:         return "Tax";
        case ActionType::ASSASSINATE: return "Assassinate";
        case ActionType::STEAL:       return "Steal";
        case ActionType::EXCHANGE:    return "Exchange";
        case ActionType::PASS:        return "Pass";
        case ActionType::CHALLENGE:   return "Challenge";
        case ActionType::BLOCK:       return "Block";
        case ActionType::LOSE_CARD:      return "LoseCard";
        case ActionType::KEEP_CARDS:     return "KeepCards";   // legacy
        case ActionType::EXCHANGE_KEEP:  return "ExchangeKeep";
        case ActionType::EXCHANGE_RETURN:return "ExchangeReturn";
        default:                         return "Unknown";
    }
}

const char* GameState::phaseName(Phase p) {
    switch (p) {
        case Phase::ACTION_SELECTION:         return "ACTION_SELECTION";
        case Phase::AWAITING_ACTION_RESPONSE: return "AWAITING_ACTION_RESPONSE";
        case Phase::AWAITING_BLOCK_RESPONSE:  return "AWAITING_BLOCK_RESPONSE";
        case Phase::EXCHANGE_SELECTION:       return "EXCHANGE_SELECTION";  // legacy
        case Phase::LOSE_INFLUENCE:           return "LOSE_INFLUENCE";
        case Phase::GAME_OVER:                return "GAME_OVER";
        case Phase::EXCHANGE_PICKING:         return "EXCHANGE_PICKING";
        case Phase::EXCHANGE_RETURNING:       return "EXCHANGE_RETURNING";
        default:                              return "UNKNOWN";
    }
}

const char* GameState::loseReasonName(LoseReason r) {
    switch (r) {
        case LoseReason::COUP_TARGET:             return "COUP_TARGET";
        case LoseReason::ASSASSINATE_TARGET:      return "ASSASSINATE_TARGET";
        case LoseReason::PROVED_ACTION:           return "PROVED_ACTION";
        case LoseReason::FAILED_CHALLENGE_ACTION: return "FAILED_CHALLENGE_ACTION";
        case LoseReason::PROVED_BLOCK:            return "PROVED_BLOCK";
        case LoseReason::FAILED_CHALLENGE_BLOCK:  return "FAILED_CHALLENGE_BLOCK";
        default:                                  return "UNKNOWN";
    }
}

// ─── Construction ────────────────────────────────────────────────────────────

GameState::GameState(const std::vector<std::string>& names, uint64_t seed)
    : rng_(seed)
{
    if (names.size() < 2 || names.size() > 6)
        throw std::invalid_argument("Coup requires 2-6 players");

    initDeck();

    players_.resize(names.size());
    for (int i = 0; i < (int)names.size(); ++i) {
        players_[i].name   = names[i];
        players_[i].coins  = 2;
        players_[i].cards[0] = { drawCard(), true };
        players_[i].cards[1] = { drawCard(), true };
    }
    hasResponded_.assign(names.size(), false);
    activePlayer_ = 0;
}

// ─── fromSetup factory ────────────────────────────────────────────────────────

GameState GameState::fromSetup(const std::vector<PlayerSetup>& setup,
                               int      activePlayerIndex,
                               uint64_t seed)
{
    if (setup.size() < 2 || setup.size() > 6)
        throw std::invalid_argument("Coup requires 2-6 players");
    if (activePlayerIndex < 0 || activePlayerIndex >= (int)setup.size())
        throw std::invalid_argument("activePlayerIndex out of range");

    // Count how many of each card are assigned to players
    // Full deck: 3 copies of each of the 5 cards = 15 cards
    int used[5] = {};
    for (auto& ps : setup) {
        for (const InfluenceCard* ic : { &ps.card0, &ps.card1 }) {
            if (ic->type == Card::NONE)
                throw std::invalid_argument("Card::NONE is not allowed in PlayerSetup");
            int idx = (int)ic->type;
            if (++used[idx] > 3)
                throw std::invalid_argument(
                    std::string("Too many copies of ") + cardName(ic->type) +
                    " (max 3 per deck)");
        }
    }

    // Build the GameState via the private default path, then overwrite fields
    // We use a trick: construct with dummy names then replace player data.
    std::vector<std::string> names;
    names.reserve(setup.size());
    for (auto& ps : setup) names.push_back(ps.name);

    // Construct with the correct number of players (this shuffles a full deck
    // and deals cards – we'll overwrite everything below).
    GameState gs(names, seed);

    // Replace each player's data with the setup values
    for (int i = 0; i < (int)setup.size(); ++i) {
        gs.players_[i].name   = setup[i].name;
        gs.players_[i].coins  = setup[i].coins;
        gs.players_[i].cards[0] = setup[i].card0;
        gs.players_[i].cards[1] = setup[i].card1;
    }

    // Rebuild the court deck from the cards NOT assigned to any player
    gs.deck_.clear();
    for (int c = 0; c < 5; ++c) {
        int remaining = 3 - used[c];
        for (int j = 0; j < remaining; ++j)
            gs.deck_.push_back(static_cast<Card>(c));
    }
    std::shuffle(gs.deck_.begin(), gs.deck_.end(), gs.rng_);

    // Set the active player and reset response tracking
    gs.activePlayer_ = activePlayerIndex;
    gs.phase_        = Phase::ACTION_SELECTION;
    gs.respondingPlayer_ = -1;
    gs.pending_      = {};
    gs.block_        = std::nullopt;
    gs.exchCards_.clear();
    gs.hasResponded_.assign(setup.size(), false);

    // If every non-active player is eliminated this is already game over
    if (gs.winner() != -1)
        gs.phase_ = Phase::GAME_OVER;

    return gs;
}

void GameState::initDeck() {
    deck_.clear();
    for (int i = 0; i < 3; ++i) {   // 3 copies of each card = 15-card court deck
        deck_.push_back(Card::DUKE);
        deck_.push_back(Card::ASSASSIN);
        deck_.push_back(Card::CAPTAIN);
        deck_.push_back(Card::AMBASSADOR);
        deck_.push_back(Card::CONTESSA);
    }
    std::shuffle(deck_.begin(), deck_.end(), rng_);
}


//TO DO:
/*
Ensure that this works properly
*/
Card GameState::drawCard() {
    if (deck_.empty())
        throw std::runtime_error("Court deck is empty - this should not happen");
    Card c = deck_.back();
    deck_.pop_back();
    return c;
}


// ─── Navigation ──────────────────────────────────────────────────────────────

int GameState::winner() const {
    int alive = -1, count = 0;
    for (int i = 0; i < (int)players_.size(); ++i) {
        if (!players_[i].isEliminated()) { alive = i; ++count; }
    }
    return (count == 1) ? alive : -1;
}

int GameState::nextAlive(int from) const {
    int n = (int)players_.size();
    for (int i = 1; i < n; ++i) {
        int idx = (from + i) % n;
        if (!players_[idx].isEliminated()) return idx;
    }
    return -1;  // only `from` is alive (game should be over)
}

void GameState::advanceTurn() {
    if (winner() != -1) {
        phase_ = Phase::GAME_OVER;
        return;
    }
    activePlayer_ = nextAlive(activePlayer_);
    phase_            = Phase::ACTION_SELECTION;
    respondingPlayer_ = -1;
    pending_          = {};
    block_            = std::nullopt;
    exchCards_.clear();
    exchKept_.clear();
    exchStep_ = 0;
    std::fill(hasResponded_.begin(), hasResponded_.end(), false);
}

void GameState::beginActionResponsePhase() {
    std::fill(hasResponded_.begin(), hasResponded_.end(), false);
    // Actor never responds to their own action
    hasResponded_[activePlayer_] = true;
    // Also skip eliminated players pre-emptively
    for (int i = 0; i < (int)players_.size(); ++i)
        if (players_[i].isEliminated()) hasResponded_[i] = true;

    // Find first responder (player after actor, going clockwise)
    respondingPlayer_ = -1;
    int n = (int)players_.size();
    for (int i = 1; i < n; ++i) {
        int idx = (activePlayer_ + i) % n;
        if (!hasResponded_[idx]) { respondingPlayer_ = idx; break; }
    }

    if (respondingPlayer_ == -1) {
        // No one can respond — action goes through immediately
        resolveAction();
    } else {
        phase_ = Phase::AWAITING_ACTION_RESPONSE;
    }
}

/// Mark current respondingPlayer_ as done and find the next.
/// Sets respondingPlayer_ = -1 if everyone has responded.
void GameState::advanceRespondingPlayer() {
    hasResponded_[respondingPlayer_] = true;
    int n = (int)players_.size();
    for (int i = 1; i < n; ++i) {
        int idx = (respondingPlayer_ + i) % n;
        if (!hasResponded_[idx]) { respondingPlayer_ = idx; return; }
    }
    respondingPlayer_ = -1;
}

// ─── Legal Moves ─────────────────────────────────────────────────────────────

std::vector<Move> GameState::legalMoves() const {
    std::vector<Move> moves;

    switch (phase_) {

    case Phase::ACTION_SELECTION: {
        const Player& p = players_[activePlayer_];

        // At 10+ coins you MUST coup
        if (p.coins >= 10) {
            for (int i = 0; i < numPlayers(); ++i)
                if (i != activePlayer_ && !players_[i].isEliminated())
                    moves.push_back(Move::coup(i));
            break;
        }

        moves.push_back(Move::income());
        moves.push_back(Move::foreignAid());
        moves.push_back(Move::tax());
        moves.push_back(Move::exchange());

        for (int i = 0; i < numPlayers(); ++i) {
            if (i == activePlayer_ || players_[i].isEliminated()) continue;
            moves.push_back(Move::steal(i));
            if (p.coins >= 3)  moves.push_back(Move::assassinate(i));
            if (p.coins >= 7)  moves.push_back(Move::coup(i));
        }
        break;
    }

    case Phase::AWAITING_ACTION_RESPONSE: {
        moves.push_back(Move::pass());
        if (pending_.challengeable)
            moves.push_back(Move::challenge());
        if (pending_.blockable) {
            for (Card c : blockingCards())
                moves.push_back(Move::block(c));
        }
        break;
    }

    case Phase::AWAITING_BLOCK_RESPONSE: {
        moves.push_back(Move::pass());
        moves.push_back(Move::challenge());
        break;
    }

    case Phase::EXCHANGE_PICKING: {
        // Offer one EXCHANGE_KEEP per card not yet claimed
        std::vector<bool> taken(exchCards_.size(), false);
        for (int i : exchKept_) taken[i] = true;
        for (int i = 0; i < (int)exchCards_.size(); ++i)
            if (!taken[i]) moves.push_back(Move::exchangeKeep(i));
        break;
    }

    case Phase::EXCHANGE_RETURNING: {
        // Offer one EXCHANGE_RETURN per card not being kept
        for (int i : exchangeReturnIndices())
            moves.push_back(Move::exchangeReturn(i));
        break;
    }

    case Phase::EXCHANGE_SELECTION: // legacy – never entered by the engine
        break;

    case Phase::LOSE_INFLUENCE: {
        const Player& p = players_[loseInfluencePid_];
        if (p.cards[0].alive) moves.push_back(Move::loseCard(0));
        if (p.cards[1].alive) moves.push_back(Move::loseCard(1));
        break;
    }

    case Phase::GAME_OVER:
        break;
    }

    return moves;
}

// ─── Apply Move (dispatcher) ─────────────────────────────────────────────────

void GameState::applyMove(const Move& m) {
    switch (phase_) {
        case Phase::ACTION_SELECTION:         applyActionSelection(m);   break;
        case Phase::AWAITING_ACTION_RESPONSE: applyActionResponse(m);    break;
        case Phase::AWAITING_BLOCK_RESPONSE:  applyBlockResponse(m);     break;
        case Phase::EXCHANGE_PICKING:         applyExchangePicking(m);   break;
        case Phase::EXCHANGE_RETURNING:       applyExchangeReturning(m); break;
        case Phase::LOSE_INFLUENCE:           applyLoseInfluence(m);     break;
        case Phase::EXCHANGE_SELECTION:  // legacy – no longer entered
            throw std::runtime_error("EXCHANGE_SELECTION is a legacy phase; should not be reached");
        case Phase::GAME_OVER:
            throw std::runtime_error("Game is over; no moves accepted");
    }
}

// ─── Phase: ACTION_SELECTION ─────────────────────────────────────────────────

void GameState::applyActionSelection(const Move& m) {
    Player& actor = players_[activePlayer_];

    switch (m.action) {
    case ActionType::INCOME:
        actor.coins += 1;
        advanceTurn();
        return;

    case ActionType::FOREIGN_AID:
        pending_ = { ActionType::FOREIGN_AID, activePlayer_, -1, Card::NONE, false, true };
        beginActionResponsePhase();
        return;

    case ActionType::COUP:
        if (actor.coins < 7)
            throw std::invalid_argument("Need 7 coins to coup");
        if (m.target < 0 || m.target >= numPlayers() || players_[m.target].isEliminated())
            throw std::invalid_argument("Invalid coup target");
        actor.coins -= 7;
        pending_ = { ActionType::COUP, activePlayer_, m.target, Card::NONE, false, false };
        enterLoseInfluence(m.target, LoseReason::COUP_TARGET, false);
        return;

    case ActionType::TAX:
        pending_ = { ActionType::TAX, activePlayer_, -1, Card::DUKE, true, false };
        beginActionResponsePhase();
        return;

    case ActionType::ASSASSINATE:
        if (actor.coins < 3)
            throw std::invalid_argument("Need 3 coins to assassinate");
        if (m.target < 0 || m.target >= numPlayers() || players_[m.target].isEliminated())
            throw std::invalid_argument("Invalid assassinate target");
        actor.coins -= 3;  // coins spent immediately, even if blocked
        pending_ = { ActionType::ASSASSINATE, activePlayer_, m.target, Card::ASSASSIN, true, true };
        beginActionResponsePhase();
        return;

    case ActionType::STEAL:
        if (m.target < 0 || m.target >= numPlayers() || players_[m.target].isEliminated())
            throw std::invalid_argument("Invalid steal target");
        if (m.target == activePlayer_)
            throw std::invalid_argument("Cannot steal from yourself");
        pending_ = { ActionType::STEAL, activePlayer_, m.target, Card::CAPTAIN, true, true };
        beginActionResponsePhase();
        return;

    case ActionType::EXCHANGE:
        pending_ = { ActionType::EXCHANGE, activePlayer_, -1, Card::AMBASSADOR, true, false };
        beginActionResponsePhase();
        return;

    default:
        throw std::invalid_argument("Invalid action in ACTION_SELECTION phase");
    }
}

// ─── Phase: AWAITING_ACTION_RESPONSE ─────────────────────────────────────────

void GameState::applyActionResponse(const Move& m) {
    switch (m.action) {

    case ActionType::PASS:
        advanceRespondingPlayer();
        if (respondingPlayer_ == -1)
            resolveAction();  // all passed → action resolves
        return;

    case ActionType::CHALLENGE: {
        int challenger = respondingPlayer_;
        bool actorHasCard = players_[pending_.actor].hasCard(pending_.claimedCard);

        if (actorHasCard) {
            // Challenge FAILED: actor reveals card, gets new one; challenger loses influence
            proveCard(pending_.actor, pending_.claimedCard);
            enterLoseInfluence(challenger, LoseReason::PROVED_ACTION, /*resolveAfter=*/true);
        } else {
            // Challenge SUCCEEDED: actor was bluffing; actor loses influence; action fails
            enterLoseInfluence(pending_.actor, LoseReason::FAILED_CHALLENGE_ACTION, false);
        }
        return;
    }

    case ActionType::BLOCK: {
        block_ = PendingBlock{ respondingPlayer_, m.card };

        // Reset response tracking for block-response phase:
        // everyone except the blocker (and eliminated players) can challenge
        std::fill(hasResponded_.begin(), hasResponded_.end(), false);
        hasResponded_[block_->blocker] = true;
        for (int i = 0; i < numPlayers(); ++i)
            if (players_[i].isEliminated()) hasResponded_[i] = true;

        // Find first potential challenger, starting from actor
        respondingPlayer_ = -1;
        int n = (int)players_.size();
        for (int i = 0; i < n; ++i) {
            int idx = (activePlayer_ + i) % n;
            if (!hasResponded_[idx]) { respondingPlayer_ = idx; break; }
        }

        if (respondingPlayer_ == -1) {
            actionFailed();  // nobody can challenge → block stands
        } else {
            phase_ = Phase::AWAITING_BLOCK_RESPONSE;
        }
        return;
    }

    default:
        throw std::invalid_argument("Invalid move in AWAITING_ACTION_RESPONSE phase");
    }
}

// ─── Phase: AWAITING_BLOCK_RESPONSE ──────────────────────────────────────────

void GameState::applyBlockResponse(const Move& m) {
    switch (m.action) {

    case ActionType::PASS:
        advanceRespondingPlayer();
        if (respondingPlayer_ == -1)
            actionFailed();  // all passed → block stands → action cancelled
        return;

    case ActionType::CHALLENGE: {
        int challenger = respondingPlayer_;
        bool blockerHasCard = players_[block_->blocker].hasCard(block_->claimedCard);

        if (blockerHasCard) {
            // Block challenge FAILED: blocker proves it; challenger loses influence; block stands
            proveCard(block_->blocker, block_->claimedCard);
            enterLoseInfluence(challenger, LoseReason::PROVED_BLOCK, false);
        } else {
            // Block challenge SUCCEEDED: blocker was bluffing; blocker loses influence;
            // the original action now resolves (resolveAfterLose_ = true)
            enterLoseInfluence(block_->blocker, LoseReason::FAILED_CHALLENGE_BLOCK, true);
        }
        return;
    }

    default:
        throw std::invalid_argument("Invalid move in AWAITING_BLOCK_RESPONSE phase");
    }
}

// ─── Phase: EXCHANGE_PICKING ──────────────────────────────────────────────────

void GameState::applyExchangePicking(const Move& m) {
    if (m.action != ActionType::EXCHANGE_KEEP)
        throw std::invalid_argument("Expected EXCHANGE_KEEP in EXCHANGE_PICKING phase");

    int idx = m.cardIdx;
    if (idx < 0 || idx >= (int)exchCards_.size())
        throw std::invalid_argument("exchangeKeep: index out of range");

    for (int k : exchKept_)
        if (k == idx)
            throw std::invalid_argument("exchangeKeep: card already selected");

    exchKept_.push_back(idx);
    ++exchStep_;

    int inf = players_[activePlayer_].influence();
    if ((int)exchKept_.size() == inf) {
        // All keeps chosen → move to return-ordering sub-phase
        auto retIdx = exchangeReturnIndices();
        if (retIdx.size() <= 1) {
            // 0 or 1 card to return → no ordering choice needed
            finishExchange(-1);
        } else {
            phase_ = Phase::EXCHANGE_RETURNING;
        }
    }
    // else remain in EXCHANGE_PICKING for the next keep-pick
}

// ─── Phase: EXCHANGE_RETURNING ────────────────────────────────────────────────

void GameState::applyExchangeReturning(const Move& m) {
    if (m.action != ActionType::EXCHANGE_RETURN)
        throw std::invalid_argument("Expected EXCHANGE_RETURN in EXCHANGE_RETURNING phase");

    int idx = m.cardIdx;
    bool valid = false;
    for (int i : exchangeReturnIndices())
        if (i == idx) { valid = true; break; }
    if (!valid)
        throw std::invalid_argument("exchangeReturn: index is not a returnable card");

    // idx is the card the actor wants on top of the deck (drawn first by others).
    finishExchange(idx);
}

// ─── Exchange helpers ─────────────────────────────────────────────────────────

/// Indices into exchCards_ that are NOT in exchKept_.
std::vector<int> GameState::exchangeReturnIndices() const {
    std::vector<bool> kept(exchCards_.size(), false);
    for (int i : exchKept_) kept[i] = true;
    std::vector<int> ret;
    for (int i = 0; i < (int)exchCards_.size(); ++i)
        if (!kept[i]) ret.push_back(i);
    return ret;
}

/// Cards still available to pick (EXCHANGE_PICKING) or being returned (EXCHANGE_RETURNING).
std::vector<Card> GameState::exchangeAvailableCards() const {
    std::vector<Card> avail;
    if (phase_ == Phase::EXCHANGE_PICKING) {
        std::vector<bool> taken(exchCards_.size(), false);
        for (int i : exchKept_) taken[i] = true;
        for (int i = 0; i < (int)exchCards_.size(); ++i)
            if (!taken[i]) avail.push_back(exchCards_[i]);
    } else if (phase_ == Phase::EXCHANGE_RETURNING) {
        for (int i : exchangeReturnIndices())
            avail.push_back(exchCards_[i]);
    } else {
        avail = exchCards_;
    }
    return avail;
}

/// Commit the exchange: assign kept cards to actor, push returned cards onto deck.
/// topChoice is the exchCards_ index placed on top (drawn first); -1 = no preference.
void GameState::finishExchange(int topChoice) {
    Player& actor = players_[activePlayer_];

    // Assign kept cards to actor's live slots in selection order
    int j = 0;
    for (int i = 0; i < 2; ++i)
        if (actor.cards[i].alive) actor.cards[i].type = exchCards_[exchKept_[j++]];

    // Return the rest to the deck.  topChoice goes on top (push last = drawn first).
    auto retIdx = exchangeReturnIndices();
    if (topChoice < 0 || retIdx.size() <= 1) {
        // No ordering preference – push all in natural index order
        for (int i : retIdx) deck_.push_back(exchCards_[i]);
    } else {
        // Push the non-top card first (buried), then topChoice (on top)
        for (int i : retIdx)
            if (i != topChoice) deck_.push_back(exchCards_[i]);
        deck_.push_back(exchCards_[topChoice]);
    }

    exchCards_.clear();
    exchKept_.clear();
    exchStep_ = 0;
    advanceTurn();
}

// ─── Phase: LOSE_INFLUENCE ───────────────────────────────────────────────────

void GameState::applyLoseInfluence(const Move& m) {
    if (m.action != ActionType::LOSE_CARD)
        throw std::invalid_argument("Expected LOSE_CARD in LOSE_INFLUENCE phase");

    int idx = m.cardIdx;
    if (idx < 0 || idx > 1)
        throw std::invalid_argument("Card index must be 0 or 1");

    Player& p = players_[loseInfluencePid_];
    if (!p.cards[idx].alive)
        throw std::invalid_argument("That card is already revealed");

    p.cards[idx].alive = false;  // reveal / lose this influence
    afterLoseInfluence();
}

void GameState::afterLoseInfluence() {
    if (winner() != -1) {
        phase_ = Phase::GAME_OVER;
        return;
    }
    if (resolveAfterLose_) {
        resolveAfterLose_ = false;
        resolveAction();
    } else {
        advanceTurn();
    }
}

// ─── Action Resolution ───────────────────────────────────────────────────────

void GameState::resolveAction() {
    Player& actor = players_[pending_.actor];

    switch (pending_.action) {

    case ActionType::INCOME:
        actor.coins += 1;
        break;

    case ActionType::FOREIGN_AID:
        actor.coins += 2;
        break;

    case ActionType::TAX:
        actor.coins += 3;
        break;

    case ActionType::STEAL: {
        Player& tgt = players_[pending_.target];
        int stolen  = std::min(tgt.coins, 2);
        tgt.coins  -= stolen;
        actor.coins += stolen;
        break;
    }

    case ActionType::ASSASSINATE:
        // Target must now choose which card to lose
        enterLoseInfluence(pending_.target, LoseReason::ASSASSINATE_TARGET, false);
        return;  // do NOT fall through to advanceTurn

    case ActionType::EXCHANGE: {
        // Collect actor's live cards + up to 2 drawn from the court deck
        exchCards_.clear();
        exchKept_.clear();
        exchStep_ = 0;
        for (int i = 0; i < 2; ++i)
            if (actor.cards[i].alive)
                exchCards_.push_back(actor.cards[i].type);
        int draws = std::min(2, (int)deck_.size());
        for (int i = 0; i < draws; ++i)
            exchCards_.push_back(drawCard());
        phase_ = Phase::EXCHANGE_PICKING;   // step-by-step exchange begins
        return;
    }

    default:
        // COUP is handled before beginActionResponsePhase; shouldn't reach here
        break;
    }

    advanceTurn();
}

void GameState::actionFailed() {
    // Block stood (or actor challenge succeeded).
    // For ASSASSINATE: coins are already spent per official rules.
    advanceTurn();
}

// ─── Utilities ───────────────────────────────────────────────────────────────

/// Actor/blocker had the claimed card → reveal it, shuffle back, draw new one.
void GameState::proveCard(int pid, Card c) {
    Player& p = players_[pid];
    for (int i = 0; i < 2; ++i) {
        if (p.cards[i].alive && p.cards[i].type == c) {
            // Shuffle old card back into the court deck
            deck_.push_back(p.cards[i].type);
            std::shuffle(deck_.begin(), deck_.end(), rng_);
            // Draw a replacement (card stays face-down / alive)
            p.cards[i].type = drawCard();
            return;
        }
    }
    // Shouldn't happen if called correctly
    throw std::logic_error("proveCard: player does not hold the claimed card");
}

void GameState::enterLoseInfluence(int pid, LoseReason reason, bool resolveAfter) {
    // If the player is already eliminated (e.g. block-challenge killed them and then
    // the assassination would also try to take a card), skip straight to resolution.
    if (players_[pid].isEliminated()) {
        resolveAfterLose_ = resolveAfter;
        loseInfluencePid_ = pid;
        loseReason_       = reason;
        afterLoseInfluence();
        return;
    }
    loseInfluencePid_ = pid;
    loseReason_       = reason;
    resolveAfterLose_ = resolveAfter;
    phase_            = Phase::LOSE_INFLUENCE;
}

/// Cards that respondingPlayer_ can legally claim to block pending_.action.
std::vector<Card> GameState::blockingCards() const {
    switch (pending_.action) {
    case ActionType::FOREIGN_AID:
        return { Card::DUKE };                          // any player
    case ActionType::STEAL:
        if (respondingPlayer_ == pending_.target)
            return { Card::CAPTAIN, Card::AMBASSADOR }; // only target
        return {};
    case ActionType::ASSASSINATE:
        if (respondingPlayer_ == pending_.target)
            return { Card::CONTESSA };                  // only target
        return {};
    default:
        return {};
    }
}

// ─── 3-D integer encoding ─────────────────────────────────────────────────────

/// Canonical mapping:  Move  →  (action, y, z) where
///   x  = (int)ActionType
///   y  = target player index   for targeted primary actions (COUP/ASSASSINATE/STEAL)
///        cardIdx                for LOSE_CARD / EXCHANGE_KEEP / EXCHANGE_RETURN
///        N (numPlayers)         otherwise  (sentinel = "unused")
///   z  = (int)Card             for actions/blocks that claim a character card
///        N                     otherwise
///
/// Every distinct legal move maps to a distinct point because:
///   • x uniquely identifies the ActionType.
///   • For each type, y and/or z uniquely identify the parameters.
std::array<int,3> GameState::moveToPoint3D(const Move& m, int N) {
    int x = (int)m.action;
    int y = N;  // sentinel: unused
    int z = N;  // sentinel: unused

    switch (m.action) {
    // Targeted primary actions (have both target and claimed card)
    case ActionType::COUP:
        y = m.target;
        break;
    case ActionType::ASSASSINATE:
    case ActionType::STEAL:
        y = m.target;
        z = (int)m.card;   // ASSASSIN or CAPTAIN
        break;
    // Untargeted character actions with a claimed card
    case ActionType::TAX:
    case ActionType::EXCHANGE:
        z = (int)m.card;   // DUKE or AMBASSADOR
        break;
    // Block uses the claimed card in z
    case ActionType::BLOCK:
        z = (int)m.card;
        break;
    // LOSE_CARD encodes the card slot index in y
    case ActionType::LOSE_CARD:
        y = m.cardIdx;
        break;
    // Exchange step-by-step: exchCards_ index in y
    case ActionType::EXCHANGE_KEEP:
    case ActionType::EXCHANGE_RETURN:
        y = m.cardIdx;
        break;
    // Legacy bitmask: mask in y (not normally generated)
    case ActionType::KEEP_CARDS:
        y = m.cardIdx;
        break;
    // INCOME / FOREIGN_AID / PASS / CHALLENGE → (x, N, N)
    default:
        break;
    }

    return { x, y, z };
}

std::vector<std::array<int,3>> GameState::legalMovesToIntegers() const {
    auto moves = legalMoves();
    int N = numPlayers();
    std::vector<std::array<int,3>> pts;
    pts.reserve(moves.size());
    for (const auto& m : moves)
        pts.push_back(moveToPoint3D(m, N));
    return pts;
}

// ─── toString ────────────────────────────────────────────────────────────────

std::string GameState::toString() const {
    std::ostringstream ss;

    ss << "╔══════════════════════════════════════╗\n";
    ss << "║        C O U P   G A M E  STATE      ║\n";
    ss << "╚══════════════════════════════════════╝\n";
    ss << "Phase   : " << phaseName(phase_) << "\n";
    ss << "Active  : " << players_[activePlayer_].name << "\n";
    if (respondingPlayer_ >= 0)
        ss << "Waiting : " << players_[respondingPlayer_].name << "\n";
    ss << "Deck    : " << deck_.size() << " cards\n\n";

    for (int i = 0; i < numPlayers(); ++i) {
        const Player& p = players_[i];
        ss << "  [" << i << "] " << p.name;
        ss << "  coins=" << p.coins;
        ss << "  cards=(";
        for (int j = 0; j < 2; ++j) {
            if (j) ss << ", ";
            if (!p.cards[j].alive) ss << "✗";
            ss << cardName(p.cards[j].type);
        }
        ss << ")";
        if (p.isEliminated()) ss << "  ── ELIMINATED";
        ss << "\n";
    }

    if (phase_ != Phase::ACTION_SELECTION && phase_ != Phase::GAME_OVER) {
        ss << "\nPending : " << actionName(pending_.action)
           << " by " << players_[pending_.actor].name;
        if (pending_.target >= 0)
            ss << " → " << players_[pending_.target].name;
        if (pending_.claimedCard != Card::NONE)
            ss << " (claiming " << cardName(pending_.claimedCard) << ")";
        ss << "\n";
    }

    if (block_) {
        ss << "Block   : " << players_[block_->blocker].name
           << " claims " << cardName(block_->claimedCard) << "\n";
    }

    if (!exchCards_.empty()) {
        ss << "Exchange pool : ";
        for (int i = 0; i < (int)exchCards_.size(); ++i) {
            bool kept = false;
            for (int k : exchKept_) if (k == i) { kept = true; break; }
            ss << "[" << i << "]" << cardName(exchCards_[i]);
            if (kept) ss << "✓";
            if (i + 1 < (int)exchCards_.size()) ss << ", ";
        }
        ss << "\n";
        if (phase_ == Phase::EXCHANGE_PICKING)
            ss << "Exchange step : PICKING  (kept " << exchKept_.size()
               << "/" << players_[activePlayer_].influence() << ")\n";
        else if (phase_ == Phase::EXCHANGE_RETURNING)
            ss << "Exchange step : RETURNING (pick which card goes on top of deck)\n";
    }

    if (phase_ == Phase::LOSE_INFLUENCE) {
        ss << "Lose influence: " << players_[loseInfluencePid_].name
           << "  reason=" << loseReasonName(loseReason_) << "\n";
    }

    if (phase_ == Phase::GAME_OVER) {
        int w = winner();
        if (w >= 0) ss << "\n🏆  WINNER: " << players_[w].name << "\n";
    }

    return ss.str();
}

} // namespace coup
