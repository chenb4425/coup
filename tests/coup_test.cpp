// ============================================================
// tests/coup_test.cpp  –  GoogleTest / GMock suite for the Coup engine
// ============================================================
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <random>

#include "coup.hpp"

using namespace coup;
using ::testing::Contains;
using ::testing::Each;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::UnorderedElementsAre;

// ─── Helpers ─────────────────────────────────────────────────────────────────

/// Play a full game to completion using random legal moves.
/// Returns the winner index.
static int playRandomGame(const std::vector<std::string>& names, uint64_t seed) {
    GameState gs(names, seed);
    std::mt19937 rng(seed ^ 0xDEADBEEFull);
    int steps = 0;
    while (!gs.isTerminal() && steps < 10'000) {
        auto moves = gs.legalMoves();
        EXPECT_FALSE(moves.empty()) << "legalMoves() returned empty in non-terminal state";
        std::uniform_int_distribution<int> d(0, (int)moves.size() - 1);
        gs.applyMove(moves[d(rng)]);
        ++steps;
    }
    EXPECT_TRUE(gs.isTerminal()) << "Game did not terminate within step budget";
    return gs.winner();
}

/// Collect all ActionType values present in a move list.
static std::vector<ActionType> actionTypes(const std::vector<Move>& moves) {
    std::vector<ActionType> out;
    out.reserve(moves.size());
    for (auto& m : moves) out.push_back(m.action);
    return out;
}

// ─── Construction ─────────────────────────────────────────────────────────────

TEST(Construction, ValidPlayerCounts) {
    EXPECT_NO_THROW(GameState({"A", "B"}, 0));
    EXPECT_NO_THROW(GameState({"A", "B", "C", "D", "E", "F"}, 0));
}

TEST(Construction, TooFewPlayers) {
    EXPECT_THROW(GameState({"Solo"}, 0), std::invalid_argument);
}

TEST(Construction, TooManyPlayers) {
    EXPECT_THROW(GameState({"A","B","C","D","E","F","G"}, 0), std::invalid_argument);
}

TEST(Construction, InitialState) {
    GameState gs({"Alice", "Bob"}, 42);
    EXPECT_EQ(gs.phase(), Phase::ACTION_SELECTION);
    EXPECT_EQ(gs.activePlayer(), 0);
    EXPECT_EQ(gs.respondingPlayer(), -1);
    EXPECT_EQ(gs.winner(), -1);
    EXPECT_FALSE(gs.isTerminal());
}

TEST(Construction, InitialCoinsAndInfluence) {
    GameState gs({"A", "B", "C"}, 0);
    for (int i = 0; i < gs.numPlayers(); ++i) {
        EXPECT_EQ(gs.player(i).coins, 2) << "Player " << i << " should start with 2 coins";
        EXPECT_EQ(gs.player(i).influence(), 2) << "Player " << i << " should start with 2 influence";
        EXPECT_FALSE(gs.player(i).isEliminated());
    }
}

// ─── Income ──────────────────────────────────────────────────────────────────

TEST(Income, GainsOneCoin) {
    GameState gs({"A", "B"}, 0);
    int before = gs.player(0).coins;
    gs.applyMove(Move::income());
    EXPECT_EQ(gs.player(0).coins, before + 1);
}

TEST(Income, AdvancesTurn) {
    GameState gs({"A", "B"}, 0);
    gs.applyMove(Move::income());
    EXPECT_EQ(gs.activePlayer(), 1);
    EXPECT_EQ(gs.phase(), Phase::ACTION_SELECTION);
}

TEST(Income, NeverNeedsResponse) {
    // Income should resolve immediately – no AWAITING_ACTION_RESPONSE phase
    GameState gs({"A", "B"}, 0);
    gs.applyMove(Move::income());
    EXPECT_NE(gs.phase(), Phase::AWAITING_ACTION_RESPONSE);
}

// ─── Foreign Aid ─────────────────────────────────────────────────────────────

TEST(ForeignAid, OpensResponseWindow) {
    GameState gs({"A", "B"}, 0);
    gs.applyMove(Move::foreignAid());
    EXPECT_EQ(gs.phase(), Phase::AWAITING_ACTION_RESPONSE);
    EXPECT_EQ(gs.respondingPlayer(), 1);
}

TEST(ForeignAid, AllPassGivesTwoCoins) {
    GameState gs({"A", "B"}, 0);
    int before = gs.player(0).coins;
    gs.applyMove(Move::foreignAid());
    gs.applyMove(Move::pass());          // B passes
    EXPECT_EQ(gs.player(0).coins, before + 2);
    EXPECT_EQ(gs.phase(), Phase::ACTION_SELECTION);
    EXPECT_EQ(gs.activePlayer(), 1);
}

TEST(ForeignAid, BlockedByDukeCancelsCoins) {
    GameState gs({"A", "B"}, 0);
    int before = gs.player(0).coins;
    gs.applyMove(Move::foreignAid());
    gs.applyMove(Move::block(Card::DUKE));   // B blocks
    gs.applyMove(Move::pass());              // A passes (accepts block)
    EXPECT_EQ(gs.player(0).coins, before);  // no coins gained
    EXPECT_EQ(gs.activePlayer(), 1);
}

TEST(ForeignAid, BlockOpensBlockResponseWindow) {
    GameState gs({"A", "B"}, 0);
    gs.applyMove(Move::foreignAid());
    gs.applyMove(Move::block(Card::DUKE));
    EXPECT_EQ(gs.phase(), Phase::AWAITING_BLOCK_RESPONSE);
    ASSERT_NE(gs.pendingBlock(), std::nullopt);
    EXPECT_EQ(gs.pendingBlock()->claimedCard, Card::DUKE);
}

TEST(ForeignAid, LegalBlockMovesContainsDuke) {
    GameState gs({"A", "B"}, 0);
    gs.applyMove(Move::foreignAid());
    // B's legal moves should include a BLOCK with Duke
    auto moves = gs.legalMoves();
    auto types  = actionTypes(moves);
    EXPECT_THAT(types, Contains(ActionType::BLOCK));
    bool hasDukeBlock = false;
    for (auto& m : moves)
        if (m.action == ActionType::BLOCK && m.card == Card::DUKE) hasDukeBlock = true;
    EXPECT_TRUE(hasDukeBlock);
}

// ─── Tax ─────────────────────────────────────────────────────────────────────

TEST(Tax, OpensResponseWindowAsChallengeableNotBlockable) {
    GameState gs({"A", "B"}, 0);
    gs.applyMove(Move::tax());
    EXPECT_EQ(gs.phase(), Phase::AWAITING_ACTION_RESPONSE);
    EXPECT_TRUE(gs.pendingAction().challengeable);
    EXPECT_FALSE(gs.pendingAction().blockable);
}

TEST(Tax, AllPassGivesThreeCoins) {
    GameState gs({"A", "B"}, 0);
    int before = gs.player(0).coins;
    gs.applyMove(Move::tax());
    gs.applyMove(Move::pass());
    EXPECT_EQ(gs.player(0).coins, before + 3);
}

TEST(Tax, LegalResponsesMustContainChallenge) {
    GameState gs({"A", "B"}, 0);
    gs.applyMove(Move::tax());
    EXPECT_THAT(actionTypes(gs.legalMoves()), Contains(ActionType::CHALLENGE));
}

TEST(Tax, LegalResponsesMustNotContainBlock) {
    GameState gs({"A", "B"}, 0);
    gs.applyMove(Move::tax());
    EXPECT_THAT(actionTypes(gs.legalMoves()), Not(Contains(ActionType::BLOCK)));
}

// ─── Assassinate ─────────────────────────────────────────────────────────────

TEST(Assassinate, RequiresThreeCoins) {
    GameState gs({"A", "B"}, 0);
    // Only 2 coins at start – assassinate should not be in legal moves
    EXPECT_THAT(actionTypes(gs.legalMoves()), Not(Contains(ActionType::ASSASSINATE)));
}

TEST(Assassinate, DeductsThreeCoinsImmediately) {
    // Give P0 enough coins via income turns
    GameState gs({"A", "B"}, 0);
    // earn coins: A income, B income, A income (= 4), B income (skip), A income (= 5 wouldn't be right)
    // Easier: do income for both until A has 3+
    while (gs.player(0).coins < 3) {
        if (gs.activePlayer() == 0) gs.applyMove(Move::income());
        else                        gs.applyMove(Move::income());
    }
    // Now it should be A's turn at some point with ≥3 coins
    // Force A's turn
    while (gs.activePlayer() != 0 && !gs.isTerminal())
        gs.applyMove(Move::income());

    if (gs.player(0).coins >= 3 && gs.activePlayer() == 0) {
        int before = gs.player(0).coins;
        gs.applyMove(Move::assassinate(1));
        EXPECT_EQ(gs.player(0).coins, before - 3);
    }
}

TEST(Assassinate, TargetCanBlockWithContessa) {
    // Set up so P0 has ≥3 coins and it's P0's turn
    GameState gs({"A", "B"}, 0);
    while (gs.player(0).coins < 3 || gs.activePlayer() != 0)
        gs.applyMove(Move::income());

    gs.applyMove(Move::assassinate(1));
    EXPECT_EQ(gs.phase(), Phase::AWAITING_ACTION_RESPONSE);

    // P1's legal moves should include BLOCK with Contessa
    bool hasContessaBlock = false;
    for (auto& m : gs.legalMoves())
        if (m.action == ActionType::BLOCK && m.card == Card::CONTESSA) hasContessaBlock = true;
    EXPECT_TRUE(hasContessaBlock);
}

TEST(Assassinate, SuccessfulAssassinationEntersLoseInfluence) {
    GameState gs({"A", "B"}, 0);
    while (gs.player(0).coins < 3 || gs.activePlayer() != 0)
        gs.applyMove(Move::income());

    gs.applyMove(Move::assassinate(1));
    // P1 passes (doesn't block or challenge)
    gs.applyMove(Move::pass());
    EXPECT_EQ(gs.phase(), Phase::LOSE_INFLUENCE);
    EXPECT_EQ(gs.loseInfluencePlayer(), 1);
    EXPECT_EQ(gs.loseReason(), LoseReason::ASSASSINATE_TARGET);
}

// ─── Steal ───────────────────────────────────────────────────────────────────

TEST(Steal, TakesUpToTwoCoinsFromTarget) {
    GameState gs({"A", "B"}, 0);
    int aCoins = gs.player(0).coins;
    int bCoins = gs.player(1).coins;
    gs.applyMove(Move::steal(1));
    gs.applyMove(Move::pass());  // B passes
    int stolen = std::min(bCoins, 2);
    EXPECT_EQ(gs.player(0).coins, aCoins + stolen);
    EXPECT_EQ(gs.player(1).coins, bCoins - stolen);
}

TEST(Steal, TargetCanBlockWithCaptainOrAmbassador) {
    GameState gs({"A", "B"}, 0);
    gs.applyMove(Move::steal(1));
    auto moves = gs.legalMoves();
    bool captainBlock = false, ambassadorBlock = false;
    for (auto& m : moves) {
        if (m.action == ActionType::BLOCK && m.card == Card::CAPTAIN)    captainBlock = true;
        if (m.action == ActionType::BLOCK && m.card == Card::AMBASSADOR) ambassadorBlock = true;
    }
    EXPECT_TRUE(captainBlock);
    EXPECT_TRUE(ambassadorBlock);
}

TEST(Steal, NonTargetCannotBlock) {
    GameState gs({"A", "B", "C"}, 0);
    gs.applyMove(Move::steal(1));
    // P1 passes; now P2 responds – P2 is not the target so no BLOCK
    gs.applyMove(Move::pass());  // P1 passes
    if (gs.respondingPlayer() == 2) {
        EXPECT_THAT(actionTypes(gs.legalMoves()), Not(Contains(ActionType::BLOCK)));
    }
}

TEST(Steal, CannotStealFromSelf) {
    GameState gs({"A", "B"}, 0);
    // steal(0) should not be in legal moves
    bool selfSteal = false;
    for (auto& m : gs.legalMoves())
        if (m.action == ActionType::STEAL && m.target == 0) selfSteal = true;
    EXPECT_FALSE(selfSteal);
}

// ─── Exchange ─────────────────────────────────────────────────────────────────

TEST(Exchange, ResolvesToExchangeSelectionPhase) {
    GameState gs({"A", "B"}, 0);
    gs.applyMove(Move::exchange());
    gs.applyMove(Move::pass());  // B passes
    EXPECT_EQ(gs.phase(), Phase::EXCHANGE_SELECTION);
}

TEST(Exchange, ExchangeCardsCountEqualsInfluencePlusTwoDraw) {
    GameState gs({"A", "B"}, 0);
    gs.applyMove(Move::exchange());
    gs.applyMove(Move::pass());
    // Actor has 2 influence → exchangeCards should have 4 entries
    EXPECT_EQ((int)gs.exchangeCards().size(), gs.player(0).influence() + 2);
}

TEST(Exchange, KeepCardsMaskMustMatchInfluenceCount) {
    GameState gs({"A", "B"}, 0);
    gs.applyMove(Move::exchange());
    gs.applyMove(Move::pass());
    int inf = gs.player(0).influence();
    auto moves = gs.legalMoves();
    for (auto& m : moves) {
        EXPECT_EQ(m.action, ActionType::KEEP_CARDS);
        EXPECT_EQ(__builtin_popcount(m.cardIdx), inf);
    }
}

TEST(Exchange, AfterKeepCardsReturnToActionSelection) {
    GameState gs({"A", "B"}, 0);
    gs.applyMove(Move::exchange());
    gs.applyMove(Move::pass());
    auto moves = gs.legalMoves();
    ASSERT_FALSE(moves.empty());
    gs.applyMove(moves[0]);
    EXPECT_EQ(gs.phase(), Phase::ACTION_SELECTION);
    EXPECT_EQ(gs.activePlayer(), 1);
}

// ─── Coup ────────────────────────────────────────────────────────────────────

TEST(Coup, RequiresSevenCoins) {
    GameState gs({"A", "B"}, 0);
    EXPECT_THAT(actionTypes(gs.legalMoves()), Not(Contains(ActionType::COUP)));
}

TEST(Coup, DeductsSevenCoinsAndEntersLoseInfluence) {
    GameState gs({"A", "B"}, 0);
    // Earn coins until A has 7
    while (gs.player(0).coins < 7 || gs.activePlayer() != 0)
        gs.applyMove(Move::income());
    int before = gs.player(0).coins;
    gs.applyMove(Move::coup(1));
    EXPECT_EQ(gs.player(0).coins, before - 7);
    EXPECT_EQ(gs.phase(), Phase::LOSE_INFLUENCE);
    EXPECT_EQ(gs.loseInfluencePlayer(), 1);
    EXPECT_EQ(gs.loseReason(), LoseReason::COUP_TARGET);
}

TEST(Coup, MandatoryAt10Coins_OnlyShowsCoupMoves) {
    // Build a game state where it's P0's turn with 10 coins by reaching a
    // known configuration through income actions.
    GameState gs({"A", "B"}, 0);
    while (gs.player(0).coins < 10 || gs.activePlayer() != 0)
        gs.applyMove(Move::income());
    auto types = actionTypes(gs.legalMoves());
    for (auto t : types)
        EXPECT_EQ(t, ActionType::COUP)
            << "At 10+ coins only COUP should be legal; got "
            << GameState::actionName(t);
}

// ─── Lose Influence ───────────────────────────────────────────────────────────

TEST(LoseInfluence, RevealedCardBecomesNotAlive) {
    GameState gs({"A", "B"}, 0);
    while (gs.player(0).coins < 7 || gs.activePlayer() != 0)
        gs.applyMove(Move::income());
    gs.applyMove(Move::coup(1));
    ASSERT_EQ(gs.phase(), Phase::LOSE_INFLUENCE);
    int infBefore = gs.player(1).influence();
    gs.applyMove(Move::loseCard(0));
    EXPECT_EQ(gs.player(1).influence(), infBefore - 1);
}

TEST(LoseInfluence, LosesBothCardsEliminatesPlayer) {
    GameState gs({"A", "B"}, 0);
    // Coup B twice to eliminate
    while (gs.player(0).coins < 7 || gs.activePlayer() != 0)
        gs.applyMove(Move::income());
    gs.applyMove(Move::coup(1));
    gs.applyMove(Move::loseCard(0));

    if (!gs.player(1).isEliminated()) {
        while (gs.player(0).coins < 7 || gs.activePlayer() != 0)
            gs.applyMove(Move::income());
        gs.applyMove(Move::coup(1));
        gs.applyMove(Move::loseCard(1));
    }

    EXPECT_TRUE(gs.player(1).isEliminated());
}

TEST(LoseInfluence, InvalidCardIndexThrows) {
    GameState gs({"A", "B"}, 0);
    while (gs.player(0).coins < 7 || gs.activePlayer() != 0)
        gs.applyMove(Move::income());
    gs.applyMove(Move::coup(1));
    EXPECT_THROW(gs.applyMove(Move::loseCard(2)), std::invalid_argument);
}

// ─── Challenge resolution ─────────────────────────────────────────────────────

TEST(Challenge, ChallengingTaxWithCorrectCardCausesChallengingPlayerToLose) {
    // Find a seed where P0 actually holds Duke at game start.
    // seed=42 → Alice: Duke, Assassin.  So if A declares Tax and B challenges,
    // B loses influence.
    GameState gs({"Alice", "Bob"}, 42);
    ASSERT_EQ(gs.player(0).cards[0].type, Card::DUKE);
    gs.applyMove(Move::tax());
    ASSERT_EQ(gs.phase(), Phase::AWAITING_ACTION_RESPONSE);
    int bInfBefore = gs.player(1).influence();
    gs.applyMove(Move::challenge());   // Bob challenges
    // Bob's challenge fails: Alice has Duke → Alice proves it, Bob loses influence
    EXPECT_EQ(gs.phase(), Phase::LOSE_INFLUENCE);
    EXPECT_EQ(gs.loseInfluencePlayer(), 1);
    EXPECT_EQ(gs.loseReason(), LoseReason::PROVED_ACTION);
    gs.applyMove(Move::loseCard(0));
    EXPECT_EQ(gs.player(1).influence(), bInfBefore - 1);
}

TEST(Challenge, ChallengingTaxWithBluffingActorCausesActorToLose) {
    // Find a seed where P0 does NOT hold Duke at start.
    // seed=0 → check cards
    GameState gs0({"A", "B"}, 0);
    bool p0HasDuke = gs0.player(0).hasCard(Card::DUKE);

    // Pick a seed accordingly
    uint64_t seed = p0HasDuke ? 1 : 0;
    GameState gs({"A", "B"}, seed);
    // Re-check
    while (gs.player(0).hasCard(Card::DUKE)) {
        ++seed;
        gs = GameState({"A", "B"}, seed);
    }

    int aInfBefore = gs.player(0).influence();
    gs.applyMove(Move::tax());         // A bluffs Duke
    gs.applyMove(Move::challenge());   // B challenges correctly
    EXPECT_EQ(gs.phase(), Phase::LOSE_INFLUENCE);
    EXPECT_EQ(gs.loseInfluencePlayer(), 0);
    EXPECT_EQ(gs.loseReason(), LoseReason::FAILED_CHALLENGE_ACTION);
    gs.applyMove(Move::loseCard(0));
    EXPECT_EQ(gs.player(0).influence(), aInfBefore - 1);
}

TEST(Challenge, SuccessfulDukeBlockChallengeResolvesOriginalAction) {
    // P0 tries Foreign Aid; P1 blocks with Duke; P0 challenges; P1 doesn't have Duke
    // → block-challenge succeeds → P1 loses influence → FA resolves (+2 for P0)
    // Find seed where P1 lacks Duke
    uint64_t seed = 0;
    while (true) {
        GameState probe({"A", "B"}, seed);
        if (!probe.player(1).hasCard(Card::DUKE)) break;
        ++seed;
    }
    GameState gs({"A", "B"}, seed);
    int aCoinsBefore = gs.player(0).coins;
    gs.applyMove(Move::foreignAid());
    gs.applyMove(Move::block(Card::DUKE));  // P1 bluffs Duke
    gs.applyMove(Move::challenge());        // P0 challenges
    // P1 loses influence (bluff exposed)
    ASSERT_EQ(gs.phase(), Phase::LOSE_INFLUENCE);
    EXPECT_EQ(gs.loseInfluencePlayer(), 1);
    EXPECT_EQ(gs.loseReason(), LoseReason::FAILED_CHALLENGE_BLOCK);
    gs.applyMove(Move::loseCard(0));
    // Foreign Aid should now have resolved
    EXPECT_EQ(gs.player(0).coins, aCoinsBefore + 2);
}

// ─── Legal moves invariants ───────────────────────────────────────────────────

TEST(LegalMoves, NeverEmptyInNonTerminalState) {
    for (uint64_t seed = 0; seed < 300; ++seed) {
        GameState gs({"X", "Y", "Z"}, seed);
        std::mt19937 rng(seed);
        int steps = 0;
        while (!gs.isTerminal() && steps < 500) {
            auto moves = gs.legalMoves();
            ASSERT_FALSE(moves.empty())
                << "Empty legal moves at step " << steps << " seed " << seed
                << " phase=" << GameState::phaseName(gs.phase());
            std::uniform_int_distribution<int> d(0, (int)moves.size() - 1);
            gs.applyMove(moves[d(rng)]);
            ++steps;
        }
    }
}

TEST(LegalMoves, EmptyInTerminalState) {
    GameState gs({"A", "B"}, 0);
    // Play until game over
    std::mt19937 rng(0);
    while (!gs.isTerminal()) {
        auto moves = gs.legalMoves();
        std::uniform_int_distribution<int> d(0, (int)moves.size() - 1);
        gs.applyMove(moves[d(rng)]);
    }
    EXPECT_THAT(gs.legalMoves(), IsEmpty());
}

TEST(LegalMoves, EliminatedPlayersSkipped) {
    // After a player is eliminated they should never appear as respondingPlayer
    GameState gs({"A", "B", "C"}, 42);
    std::mt19937 rng(42);
    while (!gs.isTerminal()) {
        if (gs.respondingPlayer() >= 0)
            EXPECT_FALSE(gs.player(gs.respondingPlayer()).isEliminated());
        auto moves = gs.legalMoves();
        std::uniform_int_distribution<int> d(0, (int)moves.size() - 1);
        gs.applyMove(moves[d(rng)]);
    }
}

// ─── Game termination + winner ────────────────────────────────────────────────

TEST(Termination, AlwaysProducesValidWinner_2Players) {
    for (uint64_t seed = 0; seed < 100; ++seed) {
        int w = playRandomGame({"A", "B"}, seed);
        EXPECT_GE(w, 0);
        EXPECT_LT(w, 2);
    }
}

TEST(Termination, AlwaysProducesValidWinner_6Players) {
    for (uint64_t seed = 0; seed < 30; ++seed) {
        int w = playRandomGame({"A","B","C","D","E","F"}, seed);
        EXPECT_GE(w, 0);
        EXPECT_LT(w, 6);
    }
}

TEST(Termination, WinnerIsNotEliminated) {
    for (uint64_t seed = 0; seed < 50; ++seed) {
        for (int np = 2; np <= 6; ++np) {
            std::vector<std::string> names;
            for (int i = 0; i < np; ++i) names.push_back("P" + std::to_string(i));
            GameState gs(names, seed);
            std::mt19937 rng(seed);
            while (!gs.isTerminal()) {
                auto moves = gs.legalMoves();
                std::uniform_int_distribution<int> d(0, (int)moves.size() - 1);
                gs.applyMove(moves[d(rng)]);
            }
            int w = gs.winner();
            ASSERT_GE(w, 0);
            EXPECT_FALSE(gs.player(w).isEliminated())
                << "Winner P" << w << " is eliminated (seed=" << seed << ", np=" << np << ")";
            // All others should be eliminated
            for (int i = 0; i < np; ++i)
                if (i != w) EXPECT_TRUE(gs.player(i).isEliminated());
        }
    }
}

// ─── Coin constraints ─────────────────────────────────────────────────────────

TEST(Coins, NeverGoNegative) {
    for (uint64_t seed = 0; seed < 50; ++seed) {
        GameState gs({"A", "B", "C"}, seed);
        std::mt19937 rng(seed);
        while (!gs.isTerminal()) {
            for (int i = 0; i < gs.numPlayers(); ++i)
                EXPECT_GE(gs.player(i).coins, 0) << "Negative coins for P" << i;
            auto moves = gs.legalMoves();
            std::uniform_int_distribution<int> d(0, (int)moves.size() - 1);
            gs.applyMove(moves[d(rng)]);
        }
    }
}

// ─── Error handling ───────────────────────────────────────────────────────────

TEST(ErrorHandling, ApplyMoveAfterGameOverThrows) {
    GameState gs({"A", "B"}, 0);
    std::mt19937 rng(0);
    while (!gs.isTerminal()) {
        auto moves = gs.legalMoves();
        std::uniform_int_distribution<int> d(0, (int)moves.size() - 1);
        gs.applyMove(moves[d(rng)]);
    }
    EXPECT_THROW(gs.applyMove(Move::income()), std::runtime_error);
}

TEST(ErrorHandling, CoupWithoutEnoughCoinsThrows) {
    GameState gs({"A", "B"}, 0);
    EXPECT_THROW(gs.applyMove(Move::coup(1)), std::invalid_argument);
}

TEST(ErrorHandling, LoseCardWithAlreadyRevealedSlotThrows) {
    GameState gs({"A", "B"}, 0);
    while (gs.player(0).coins < 7 || gs.activePlayer() != 0)
        gs.applyMove(Move::income());
    gs.applyMove(Move::coup(1));
    gs.applyMove(Move::loseCard(0));  // valid
    // Give A 7 more coins, coup again
    while (gs.player(0).coins < 7 || gs.activePlayer() != 0)
        gs.applyMove(Move::income());
    if (!gs.player(1).isEliminated()) {
        gs.applyMove(Move::coup(1));
        EXPECT_THROW(gs.applyMove(Move::loseCard(0)), std::invalid_argument);  // already revealed
    }
}

// ─── String helpers ───────────────────────────────────────────────────────────

TEST(StringHelpers, CardNameRoundTrip) {
    EXPECT_STREQ(GameState::cardName(Card::DUKE),       "Duke");
    EXPECT_STREQ(GameState::cardName(Card::ASSASSIN),   "Assassin");
    EXPECT_STREQ(GameState::cardName(Card::CAPTAIN),    "Captain");
    EXPECT_STREQ(GameState::cardName(Card::AMBASSADOR), "Ambassador");
    EXPECT_STREQ(GameState::cardName(Card::CONTESSA),   "Contessa");
}

TEST(StringHelpers, ToStringDoesNotCrashInAnyPhase) {
    GameState gs({"A", "B"}, 0);
    std::mt19937 rng(0);
    while (!gs.isTerminal()) {
        EXPECT_NO_THROW(gs.toString());
        auto moves = gs.legalMoves();
        std::uniform_int_distribution<int> d(0, (int)moves.size() - 1);
        gs.applyMove(moves[d(rng)]);
    }
    EXPECT_NO_THROW(gs.toString());
}
