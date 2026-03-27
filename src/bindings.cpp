// ============================================================
// bindings.cpp  –  pybind11 Python bindings for the Coup engine
// ============================================================
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/operators.h>
#include "coup.hpp"

namespace py = pybind11;
using namespace coup;

// Helper: build a repr string from card name + alive flag
static std::string cardRepr(const InfluenceCard& c) {
    std::string s = GameState::cardName(c.type);
    if (!c.alive) s = "[" + s + "]";
    return s;
}

PYBIND11_MODULE(coup_py, m) {
    m.doc() = R"doc(
coup_py – Python bindings for the Coup card-game state engine.

Quick-start
-----------
>>> import coup_py as coup
>>> gs = coup.GameState(["Alice", "Bob", "Charlie"], seed=42)
>>> while not gs.is_terminal:
...     moves = gs.legal_moves()
...     gs.apply_move(moves[0])   # always pass / first legal move
>>> print(gs)
)doc";

    // ── Enumerations ──────────────────────────────────────────────────────────

    py::enum_<Card>(m, "Card", "One of the five Coup character cards")
        .value("DUKE",       Card::DUKE)
        .value("ASSASSIN",   Card::ASSASSIN)
        .value("CAPTAIN",    Card::CAPTAIN)
        .value("AMBASSADOR", Card::AMBASSADOR)
        .value("CONTESSA",   Card::CONTESSA)
        .value("NONE",       Card::NONE)
        .export_values();

    py::enum_<ActionType>(m, "ActionType",
        "All move types; primary actions plus response / resolution moves")
        .value("INCOME",      ActionType::INCOME)
        .value("FOREIGN_AID", ActionType::FOREIGN_AID)
        .value("COUP",        ActionType::COUP)
        .value("TAX",         ActionType::TAX)
        .value("ASSASSINATE", ActionType::ASSASSINATE)
        .value("STEAL",       ActionType::STEAL)
        .value("EXCHANGE",    ActionType::EXCHANGE)
        .value("PASS",        ActionType::PASS)
        .value("CHALLENGE",   ActionType::CHALLENGE)
        .value("BLOCK",       ActionType::BLOCK)
        .value("LOSE_CARD",       ActionType::LOSE_CARD)
        .value("KEEP_CARDS",      ActionType::KEEP_CARDS)
        .value("EXCHANGE_KEEP",   ActionType::EXCHANGE_KEEP)
        .value("EXCHANGE_RETURN", ActionType::EXCHANGE_RETURN)
        .export_values();

    py::enum_<Phase>(m, "Phase", "State-machine phase of the game")
        .value("ACTION_SELECTION",         Phase::ACTION_SELECTION)
        .value("AWAITING_ACTION_RESPONSE", Phase::AWAITING_ACTION_RESPONSE)
        .value("AWAITING_BLOCK_RESPONSE",  Phase::AWAITING_BLOCK_RESPONSE)
        .value("EXCHANGE_SELECTION",       Phase::EXCHANGE_SELECTION)
        .value("LOSE_INFLUENCE",           Phase::LOSE_INFLUENCE)
        .value("GAME_OVER",                Phase::GAME_OVER)
        .value("EXCHANGE_PICKING",         Phase::EXCHANGE_PICKING)
        .value("EXCHANGE_RETURNING",       Phase::EXCHANGE_RETURNING)
        .export_values();

    py::enum_<LoseReason>(m, "LoseReason",
        "Why a player is currently being asked to lose influence")
        .value("COUP_TARGET",             LoseReason::COUP_TARGET)
        .value("ASSASSINATE_TARGET",      LoseReason::ASSASSINATE_TARGET)
        .value("PROVED_ACTION",           LoseReason::PROVED_ACTION)
        .value("FAILED_CHALLENGE_ACTION", LoseReason::FAILED_CHALLENGE_ACTION)
        .value("PROVED_BLOCK",            LoseReason::PROVED_BLOCK)
        .value("FAILED_CHALLENGE_BLOCK",  LoseReason::FAILED_CHALLENGE_BLOCK)
        .export_values();

    // ── InfluenceCard ─────────────────────────────────────────────────────────

    py::class_<InfluenceCard>(m, "InfluenceCard",
        "One of a player's two influence cards")
        .def_readonly("type",  &InfluenceCard::type,
            "The card character (Card enum)")
        .def_readonly("alive", &InfluenceCard::alive,
            "True = face-down (influence intact); False = face-up (lost)")
        .def("__repr__", [](const InfluenceCard& c) {
            return "<InfluenceCard " + cardRepr(c) + ">";
        });

    // ── Player ────────────────────────────────────────────────────────────────

    py::class_<Player>(m, "Player")
        .def_readonly("name",  &Player::name)
        .def_readonly("coins", &Player::coins)
        .def_property_readonly("card0",
            [](const Player& p) { return p.cards[0]; },
            "First influence card")
        .def_property_readonly("card1",
            [](const Player& p) { return p.cards[1]; },
            "Second influence card")
        .def("is_eliminated", &Player::isEliminated,
            "True when both influence cards are lost")
        .def("influence", &Player::influence,
            "Number of remaining (face-down) cards (0, 1, or 2)")
        .def("has_card", &Player::hasCard,
            py::arg("card"),
            "True if the player holds the given card face-down")
        .def("__repr__", [](const Player& p) {
            return "<Player " + p.name + " coins=" + std::to_string(p.coins) +
                   " inf=" + std::to_string(p.influence()) + ">";
        });

    // ── Move ──────────────────────────────────────────────────────────────────

    py::class_<Move>(m, "Move", "A move submitted to GameState.apply_move()")
        .def(py::init<>())
        .def_readwrite("action",   &Move::action)
        .def_readwrite("target",   &Move::target,
            "Target player index for COUP / ASSASSINATE / STEAL (-1 otherwise)")
        .def_readwrite("card",     &Move::card,
            "Card claimed for character actions or BLOCK (Card.NONE otherwise)")
        .def_readwrite("card_idx", &Move::cardIdx,
            "Card slot (0/1) for LOSE_CARD; keep-bitmask for KEEP_CARDS")
        // Factories
        .def_static("income",      &Move::income,      "Income: +1 coin")
        .def_static("foreign_aid", &Move::foreignAid,  "Foreign Aid: +2 coins (blockable)")
        .def_static("coup",        &Move::coup,        py::arg("target"),
            "Coup target player for 7 coins (mandatory at 10+ coins)")
        .def_static("tax",         &Move::tax,         "Tax (Duke): +3 coins")
        .def_static("assassinate", &Move::assassinate, py::arg("target"),
            "Assassinate (Assassin): pay 3 coins, target loses influence")
        .def_static("steal",       &Move::steal,       py::arg("target"),
            "Steal (Captain): take up to 2 coins from target")
        .def_static("exchange",    &Move::exchange,    "Exchange (Ambassador): swap cards")
        .def_static("pass_",       &Move::pass,        "Pass / do nothing")
        .def_static("challenge",   &Move::challenge,   "Challenge the current claim")
        .def_static("block",       &Move::block,       py::arg("claimed_card"),
            "Block the current action, claiming to hold claimed_card")
        .def_static("lose_card",       &Move::loseCard,       py::arg("card_index"),
            "Reveal card at index 0 or 1 to lose influence")
        .def_static("keep_cards",      &Move::keepCards,      py::arg("bitmask"),
            "Legacy bitmask keep; prefer exchange_keep")
        .def_static("exchange_keep",   &Move::exchangeKeep,   py::arg("card_index"),
            "EXCHANGE_PICKING phase: keep exchCards[card_index]")
        .def_static("exchange_return", &Move::exchangeReturn, py::arg("card_index"),
            "EXCHANGE_RETURNING phase: nominate exchCards[card_index] to go on top of deck")
        .def("__repr__", [](const Move& mv) {
            std::string s = "<Move ";
            s += GameState::actionName(mv.action);
            if (mv.target >= 0)   s += " tgt=" + std::to_string(mv.target);
            if (mv.card != Card::NONE)
                s += std::string(" card=") + GameState::cardName(mv.card);
            if (mv.cardIdx >= 0)  s += " idx=" + std::to_string(mv.cardIdx);
            s += ">";
            return s;
        });

    // ── PendingAction ─────────────────────────────────────────────────────────

    py::class_<PendingAction>(m, "PendingAction",
        "The action currently being resolved (challenge / block window open)")
        .def_readonly("action",       &PendingAction::action)
        .def_readonly("actor",        &PendingAction::actor,
            "Index of the player who declared the action")
        .def_readonly("target",       &PendingAction::target,
            "Index of the targeted player (-1 for untargeted actions)")
        .def_readonly("claimed_card", &PendingAction::claimedCard,
            "Card the actor is claiming to hold (Card.NONE for always-available actions)")
        .def_readonly("challengeable",&PendingAction::challengeable)
        .def_readonly("blockable",    &PendingAction::blockable)
        .def("__repr__", [](const PendingAction& pa) {
            return std::string("<PendingAction ") + GameState::actionName(pa.action) + ">";
        });

    // ── PendingBlock ──────────────────────────────────────────────────────────

    py::class_<PendingBlock>(m, "PendingBlock",
        "A block that has been declared and is now open to challenge")
        .def_readonly("blocker",      &PendingBlock::blocker,
            "Index of the blocking player")
        .def_readonly("claimed_card", &PendingBlock::claimedCard,
            "Card the blocker is claiming to hold")
        .def("__repr__", [](const PendingBlock& pb) {
            return std::string("<PendingBlock ") +
                   GameState::cardName(pb.claimedCard) + ">";
        });

    // ── GameState ─────────────────────────────────────────────────────────────

    py::class_<GameState>(m, "GameState",
        R"doc(
Complete Coup game state.

Phases and whose turn it is to move
-------------------------------------
ACTION_SELECTION        → active_player picks an action
AWAITING_ACTION_RESPONSE → responding_player passes / challenges / blocks
AWAITING_BLOCK_RESPONSE  → responding_player passes / challenges the block
EXCHANGE_SELECTION      → active_player picks which cards to keep (keep_cards bitmask)
LOSE_INFLUENCE          → lose_influence_player picks which card to reveal (lose_card 0/1)
GAME_OVER               → no more moves; check .winner
)doc")
        .def(py::init<const std::vector<std::string>&, uint64_t>(),
             py::arg("player_names"),
             py::arg("seed") = 42,
             "Construct a new game.  seed controls deck shuffle RNG.")

        // Core interface
        .def("apply_move",  &GameState::applyMove,  py::arg("move"),
            "Submit a move for the current responding / active player.")
        .def("legal_moves", &GameState::legalMoves,
            "Return all legal Move objects for the current responding / active player.")
        .def("legal_moves_to_integers", &GameState::legalMovesToIntegers,
            R"doc(
Map every current legal move to a unique 3-D integer point [x, y, z].
  x = int(ActionType)
  y = target player index / card slot / exchCards index; numPlayers() when unused
  z = int(Card) for actions that claim a card; numPlayers() when unused
Returned list is parallel to legal_moves().
)doc")

        // Phase / turn info
        .def_property_readonly("phase",               &GameState::phase)
        .def_property_readonly("active_player",       &GameState::activePlayer,
            "Index of the player whose overall turn it is.")
        .def_property_readonly("responding_player",   &GameState::respondingPlayer,
            "Index of the player currently being asked to respond (-1 if none).")
        .def_property_readonly("lose_influence_player",&GameState::loseInfluencePlayer,
            "Index of the player who must choose a card to lose (-1 if not applicable).")
        .def_property_readonly("lose_reason",         &GameState::loseReason)

        // Outcome
        .def_property_readonly("winner",      &GameState::winner,
            "Winning player index, or -1 while the game is ongoing.")
        .def_property_readonly("is_terminal", &GameState::isTerminal)

        // Players
        .def_property_readonly("players",     &GameState::players)
        .def("player", &GameState::player, py::arg("index"),
            "Return the Player at the given index.")
        .def_property_readonly("num_players", &GameState::numPlayers)

        // Pending state
        .def_property_readonly("pending_action", &GameState::pendingAction,
            "The PendingAction currently open to challenge/block.")
        .def_property_readonly("pending_block",
            [](const GameState& gs) -> py::object {
                const auto& ob = gs.pendingBlock();
                if (ob.has_value()) return py::cast(*ob);
                return py::none();
            },
            "The PendingBlock currently open to challenge, or None.")
        .def_property_readonly("exchange_cards", &GameState::exchangeCards,
            "Full card pool visible during an exchange (actor's live cards + 2 drawn).")
        .def_property_readonly("exchange_step", &GameState::exchangeStep,
            "0 = first EXCHANGE_KEEP, 1 = second EXCHANGE_KEEP, 2 = EXCHANGE_RETURNING.")
        .def_property_readonly("exchange_kept_indices", &GameState::exchangeKeptIndices,
            "Indices into exchange_cards that have been selected to keep so far.")
        .def("exchange_available_cards", &GameState::exchangeAvailableCards,
            "Cards still pickable (EXCHANGE_PICKING) or about to be returned (EXCHANGE_RETURNING).")

        // String helpers
        .def("__str__",  &GameState::toString)
        .def("__repr__", [](const GameState& gs) {
            return std::string("<GameState phase=") +
                   GameState::phaseName(gs.phase()) + ">";
        })

        // Static utility helpers exposed to Python
        .def_static("card_name",        &GameState::cardName)
        .def_static("action_name",      &GameState::actionName)
        .def_static("phase_name",       &GameState::phaseName)
        .def_static("lose_reason_name", &GameState::loseReasonName)
        .def_static("move_to_point3d",  &GameState::moveToPoint3D,
            py::arg("move"), py::arg("num_players"),
            "Encode any Move as a 3-D integer point using the same scheme as legal_moves_to_integers().");

    // ── PlayerSetup ───────────────────────────────────────────────────────────

    py::class_<PlayerSetup>(m, "PlayerSetup",
        "Describes one player's starting hand for use with GameState.from_setup().")
        .def(py::init<>())
        .def_readwrite("name",  &PlayerSetup::name)
        .def_readwrite("coins", &PlayerSetup::coins)
        .def_readwrite("card0", &PlayerSetup::card0)
        .def_readwrite("card1", &PlayerSetup::card1)
        .def_static("make",
            [](std::string n, Card c0, Card c1, int coins) {
                return PlayerSetup::make(std::move(n), c0, c1, coins);
            },
            py::arg("name"), py::arg("card0"), py::arg("card1"), py::arg("coins") = 2,
            "Both cards face-down (full influence).")
        .def_static("one_down",
            [](std::string n, Card live, Card dead, int coins) {
                return PlayerSetup::oneDown(std::move(n), live, dead, coins);
            },
            py::arg("name"), py::arg("live"), py::arg("dead"), py::arg("coins") = 2,
            "One live card, one already-revealed card (1 influence remaining).")
        .def_static("eliminated",
            [](std::string n, Card c0, Card c1) {
                return PlayerSetup::eliminated(std::move(n), c0, c1);
            },
            py::arg("name"), py::arg("card0"), py::arg("card1"),
            "Both cards already revealed – player enters pre-eliminated.")
        .def("__repr__", [](const PlayerSetup& ps) {
            return "<PlayerSetup " + ps.name + " coins=" + std::to_string(ps.coins) + ">";
        });

    // fromSetup static factory on GameState
    // (added here after PlayerSetup is registered)
    py::type::handle_of<GameState>()
        .attr("from_setup") = py::cpp_function(
            [](const std::vector<PlayerSetup>& setup, int active, uint64_t seed) {
                return GameState::fromSetup(setup, active, seed);
            },
            py::arg("player_setups"),
            py::arg("active_player_index") = 0,
            py::arg("seed") = 42,
            "Build a GameState from an explicit per-player hand description.");
}
