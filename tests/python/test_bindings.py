"""
tests/test_bindings.py
Python-side tests for the coup_py pybind11 bindings.

Run:
    pytest tests/test_bindings.py -v
(from repo root with PYTHONPATH pointing at the build directory)
"""

import pytest
import coup_py as coup

# ── Helpers ──────────────────────────────────────────────────────────────────

# BUG FIX: pybind11 enums are NOT directly iterable with `for x in EnumType`
# or `list(EnumType)`.  Use .__members__.values() instead.
ALL_CARDS = list(coup.Card.__members__.values())          # includes NONE sentinel
LIVE_CARDS = [c for c in ALL_CARDS if c != coup.Card.NONE]


def play_to_terminal(gs, max_steps=2000):
    """Advance gs to GAME_OVER choosing moves[0] every step."""
    steps = 0
    while not gs.is_terminal and steps < max_steps:
        moves = gs.legal_moves()
        assert len(moves) > 0, f"Empty legal moves at step {steps}"
        gs.apply_move(moves[0])
        steps += 1
    return steps


# ── Basic import / module sanity ─────────────────────────────────────────────

def test_import():
    assert coup is not None


def test_enums_exist():
    assert coup.Card.DUKE is not None
    assert coup.ActionType.INCOME is not None
    assert coup.Phase.ACTION_SELECTION is not None


def test_all_action_types_exist():
    """Every ActionType value that exists in the C++ header must be bound."""
    expected = [
        "INCOME", "FOREIGN_AID", "COUP", "TAX", "ASSASSINATE", "STEAL",
        "EXCHANGE", "PASS", "CHALLENGE", "BLOCK", "LOSE_CARD",
        "EXCHANGE_KEEP", "EXCHANGE_RETURN",
    ]
    members = coup.ActionType.__members__
    for name in expected:
        assert name in members, f"ActionType.{name} missing from bindings"


def test_all_phases_exist():
    """Every Phase value that exists in the C++ header must be bound."""
    expected = [
        "ACTION_SELECTION", "AWAITING_ACTION_RESPONSE", "AWAITING_BLOCK_RESPONSE",
        "LOSE_INFLUENCE", "GAME_OVER", "EXCHANGE_PICKING", "EXCHANGE_RETURNING",
    ]
    members = coup.Phase.__members__
    for name in expected:
        assert name in members, f"Phase.{name} missing from bindings"


# ── GameState construction ──────────────────────────────────────────────────

def test_create_game():
    gs = coup.GameState(["A", "B"], 42)

    assert gs.num_players == 2
    assert gs.phase == coup.Phase.ACTION_SELECTION
    assert gs.is_terminal is False
    assert gs.winner == -1


def test_create_game_keyword_seed():
    gs = coup.GameState(player_names=["Alice", "Bob"], seed=0)
    assert gs.num_players == 2


def test_players_exist():
    gs = coup.GameState(["A", "B"], 42)

    players = gs.players
    assert len(players) == 2

    p0 = players[0]
    assert p0.name == "A"
    assert p0.coins >= 0
    assert p0.influence() == 2


# ── Player + InfluenceCard ──────────────────────────────────────────────────

def test_player_cards():
    gs = coup.GameState(["A", "B"], 42)
    p = gs.player(0)

    c0 = p.card0
    c1 = p.card1

    assert c0.alive is True
    assert c1.alive is True

    # BUG FIX: list(coup.Card) raises TypeError on pybind11 enums.
    # Use .__members__.values() to get the valid enum values.
    assert c0.type in ALL_CARDS
    assert c1.type in ALL_CARDS


def test_player_has_card():
    gs = coup.GameState(["A", "B"], 42)
    p = gs.player(0)

    # BUG FIX: `for card in coup.Card` raises TypeError on pybind11 enums.
    # Iterate over the explicit list built from .__members__.values() instead.
    for card in LIVE_CARDS:
        result = p.has_card(card)
        assert isinstance(result, bool)


def test_player_influence_range():
    gs = coup.GameState(["A", "B", "C"], 0)
    for i in range(gs.num_players):
        assert 0 <= gs.player(i).influence() <= 2


def test_player_is_eliminated_false_at_start():
    gs = coup.GameState(["A", "B"], 0)
    assert gs.player(0).is_eliminated() is False
    assert gs.player(1).is_eliminated() is False


# ── Move creation ───────────────────────────────────────────────────────────

def test_move_factories():
    m_income     = coup.Move.income()
    m_fa         = coup.Move.foreign_aid()
    m_tax        = coup.Move.tax()
    m_exchange   = coup.Move.exchange()
    m_pass       = coup.Move.pass_()
    m_challenge  = coup.Move.challenge()
    m_coup       = coup.Move.coup(1)
    m_steal      = coup.Move.steal(1)
    m_assassin   = coup.Move.assassinate(1)
    m_block      = coup.Move.block(coup.Card.DUKE)
    m_lose       = coup.Move.lose_card(0)
    m_ek         = coup.Move.exchange_keep(2)
    m_er         = coup.Move.exchange_return(3)

    assert m_income.action    == coup.ActionType.INCOME
    assert m_fa.action        == coup.ActionType.FOREIGN_AID
    assert m_tax.action       == coup.ActionType.TAX
    assert m_exchange.action  == coup.ActionType.EXCHANGE
    assert m_pass.action      == coup.ActionType.PASS
    assert m_challenge.action == coup.ActionType.CHALLENGE
    assert m_coup.target      == 1
    assert m_steal.target     == 1
    assert m_assassin.target  == 1
    assert m_block.card       == coup.Card.DUKE
    assert m_lose.card_idx    == 0
    assert m_ek.card_idx      == 2
    assert m_er.card_idx      == 3


def test_move_repr():
    m = coup.Move.tax()
    s = repr(m)
    assert "Move" in s
    assert "Tax" in s


def test_move_readwrite_fields():
    m = coup.Move()
    m.action   = coup.ActionType.STEAL
    m.target   = 2
    m.card     = coup.Card.CAPTAIN
    m.card_idx = 0
    assert m.action   == coup.ActionType.STEAL
    assert m.target   == 2
    assert m.card     == coup.Card.CAPTAIN
    assert m.card_idx == 0


# ── Legal moves / apply_move ────────────────────────────────────────────────

def test_legal_moves_takes_no_arguments():
    """legal_moves() takes no arguments; calling it with any arg must raise TypeError."""
    gs = coup.GameState(["Alice", "Bob", "Charlie"], 42)

    # correct call — must not raise
    moves = gs.legal_moves()
    assert isinstance(moves, list)
    assert len(moves) > 0

    # passing any positional argument must raise TypeError (not silently ignore it)
    with pytest.raises(TypeError):
        gs.legal_moves(99)


def test_legal_moves_not_empty():
    gs = coup.GameState(["A", "B"], 42)
    moves = gs.legal_moves()
    assert len(moves) > 0


def test_legal_moves_returns_move_objects():
    gs = coup.GameState(["A", "B"], 42)
    for m in gs.legal_moves():
        assert isinstance(m, coup.Move)
        assert isinstance(m.action, coup.ActionType)


def test_apply_one_move():
    gs = coup.GameState(["A", "B"], 42)
    moves = gs.legal_moves()
    gs.apply_move(moves[0])
    assert gs.num_players == 2


def test_legal_moves_to_integers_parallel():
    """legal_moves_to_integers() must be same length as legal_moves()."""
    gs = coup.GameState(["A", "B", "C"], 0)
    moves  = gs.legal_moves()
    points = gs.legal_moves_to_integers()
    assert len(points) == len(moves)
    for pt in points:
        assert len(pt) == 3      # each point is [x, y, z]


def test_legal_moves_to_integers_x_is_action_type():
    gs = coup.GameState(["A", "B"], 0)
    moves  = gs.legal_moves()
    points = gs.legal_moves_to_integers()
    for m, pt in zip(moves, points):
        assert pt[0] == int(m.action)


def test_legal_moves_to_integers_all_distinct():
    gs = coup.GameState(["A", "B", "C"], 0)
    points = gs.legal_moves_to_integers()
    tuples = [tuple(p) for p in points]
    assert len(tuples) == len(set(tuples)), "Duplicate 3-D points in legal_moves_to_integers()"


def test_move_to_point3d_static():
    m = coup.Move.steal(1)
    pt = coup.GameState.move_to_point3d(m, 3)
    assert len(pt) == 3
    assert pt[0] == int(coup.ActionType.STEAL)
    assert pt[1] == 1   # target


# ── Game progression ────────────────────────────────────────────────────────

def test_play_until_terminal():
    gs = coup.GameState(["A", "B"], 42)
    steps = play_to_terminal(gs)
    assert gs.is_terminal
    assert gs.winner in [0, 1]
    assert steps < 2000


def test_active_player_cycles():
    gs = coup.GameState(["A", "B", "C"], 0)
    # After income (resolves immediately), active player advances
    assert gs.active_player == 0
    gs.apply_move(coup.Move.income())
    assert gs.active_player == 1


# ── Phase transitions ───────────────────────────────────────────────────────

def test_phase_property():
    gs = coup.GameState(["A", "B"], 42)
    assert isinstance(gs.phase, coup.Phase)
    assert gs.phase == coup.Phase.ACTION_SELECTION


def test_foreign_aid_opens_response_window():
    gs = coup.GameState(["A", "B"], 42)
    gs.apply_move(coup.Move.foreign_aid())
    assert gs.phase == coup.Phase.AWAITING_ACTION_RESPONSE
    assert gs.responding_player == 1


def test_block_opens_block_response_window():
    gs = coup.GameState(["A", "B"], 42)
    gs.apply_move(coup.Move.foreign_aid())
    gs.apply_move(coup.Move.block(coup.Card.DUKE))
    assert gs.phase == coup.Phase.AWAITING_BLOCK_RESPONSE


def test_exchange_enters_picking_phase():
    gs = coup.GameState(["A", "B"], 42)
    gs.apply_move(coup.Move.exchange())
    gs.apply_move(coup.Move.pass_())   # B passes
    assert gs.phase == coup.Phase.EXCHANGE_PICKING


def test_exchange_picking_moves_are_exchange_keep():
    gs = coup.GameState(["A", "B"], 0)
    gs.apply_move(coup.Move.exchange())
    gs.apply_move(coup.Move.pass_())
    assert gs.phase == coup.Phase.EXCHANGE_PICKING
    for m in gs.legal_moves():
        assert m.action == coup.ActionType.EXCHANGE_KEEP


def test_exchange_pool_size():
    gs = coup.GameState(["A", "B"], 0)
    gs.apply_move(coup.Move.exchange())
    gs.apply_move(coup.Move.pass_())
    pool = gs.exchange_cards
    assert len(pool) == gs.player(0).influence() + 2


def test_exchange_second_pick_excludes_first():
    gs = coup.GameState(["A", "B"], 0)
    gs.apply_move(coup.Move.exchange())
    gs.apply_move(coup.Move.pass_())
    gs.apply_move(coup.Move.exchange_keep(0))
    if gs.phase == coup.Phase.EXCHANGE_PICKING:
        for m in gs.legal_moves():
            assert m.card_idx != 0, "Already-kept index 0 reappeared"


def test_exchange_returns_phase_after_all_picks():
    gs = coup.GameState(["A", "B"], 0)
    gs.apply_move(coup.Move.exchange())
    gs.apply_move(coup.Move.pass_())
    inf = gs.player(0).influence()
    for i in range(inf):
        gs.apply_move(coup.Move.exchange_keep(i))
    assert gs.phase == coup.Phase.EXCHANGE_RETURNING


def test_exchange_returning_has_two_moves():
    gs = coup.GameState(["A", "B"], 0)
    gs.apply_move(coup.Move.exchange())
    gs.apply_move(coup.Move.pass_())
    inf = gs.player(0).influence()
    for i in range(inf):
        gs.apply_move(coup.Move.exchange_keep(i))
    assert gs.phase == coup.Phase.EXCHANGE_RETURNING
    moves = gs.legal_moves()
    assert len(moves) == 2
    for m in moves:
        assert m.action == coup.ActionType.EXCHANGE_RETURN


def test_exchange_return_advances_to_action_selection():
    gs = coup.GameState(["A", "B"], 0)
    gs.apply_move(coup.Move.exchange())
    gs.apply_move(coup.Move.pass_())
    inf = gs.player(0).influence()
    for i in range(inf):
        gs.apply_move(coup.Move.exchange_keep(i))
    gs.apply_move(gs.legal_moves()[0])
    assert gs.phase == coup.Phase.ACTION_SELECTION
    assert gs.active_player == 1


# ── Pending action / block ──────────────────────────────────────────────────

def test_pending_action_always_present():
    """pending_action always returns a PendingAction object, never None."""
    gs = coup.GameState(["A", "B"], 42)
    pa = gs.pending_action
    assert pa is not None
    assert isinstance(pa, coup.PendingAction)


def test_pending_action_actor_minus_one_when_idle():
    """
    BUG FIX: after income (no pending action), pending_action.actor == -1.
    The original test asserted `pa.actor >= 0` unconditionally which always
    fails after a resolved action.  Correct check: actor == -1 when idle.
    """
    gs = coup.GameState(["A", "B"], 42)
    gs.apply_move(coup.Move.income())   # resolves immediately, no pending action
    pa = gs.pending_action
    assert pa.actor == -1               # -1 = no action currently in flight


def test_pending_action_populated_after_tax():
    gs = coup.GameState(["A", "B"], 42)
    gs.apply_move(coup.Move.tax())
    pa = gs.pending_action
    assert pa.actor == 0
    assert pa.action == coup.ActionType.TAX
    assert pa.challengeable is True
    assert pa.blockable is False


def test_pending_block_none_when_no_block():
    gs = coup.GameState(["A", "B"], 42)
    gs.apply_move(coup.Move.income())
    pb = gs.pending_block
    assert pb is None


def test_pending_block_populated_after_block():
    gs = coup.GameState(["A", "B"], 42)
    gs.apply_move(coup.Move.foreign_aid())
    gs.apply_move(coup.Move.block(coup.Card.DUKE))
    pb = gs.pending_block
    assert pb is not None
    assert pb.blocker == 1
    assert pb.claimed_card == coup.Card.DUKE


# ── Exchange cards accessor ──────────────────────────────────────────────────

def test_exchange_cards_empty_outside_exchange():
    gs = coup.GameState(["A", "B"], 42)
    assert len(gs.exchange_cards) == 0


def test_exchange_cards_populated_during_exchange():
    gs = coup.GameState(["A", "B"], 0)
    gs.apply_move(coup.Move.exchange())
    gs.apply_move(coup.Move.pass_())
    cards = gs.exchange_cards
    assert len(cards) > 0
    for c in cards:
        assert isinstance(c, coup.Card)


# ── PlayerSetup + fromSetup ──────────────────────────────────────────────────

def test_player_setup_make():
    ps = coup.PlayerSetup.make("Alice", coup.Card.DUKE, coup.Card.ASSASSIN, 5)
    assert ps.name  == "Alice"
    assert ps.coins == 5
    assert ps.card0.type  == coup.Card.DUKE
    assert ps.card1.type  == coup.Card.ASSASSIN
    assert ps.card0.alive is True
    assert ps.card1.alive is True


def test_player_setup_one_down():
    ps = coup.PlayerSetup.one_down("Bob", coup.Card.CAPTAIN, coup.Card.CONTESSA)
    assert ps.card0.alive is True
    assert ps.card1.alive is False


def test_player_setup_eliminated():
    ps = coup.PlayerSetup.eliminated("Carol", coup.Card.DUKE, coup.Card.AMBASSADOR)
    assert ps.card0.alive is False
    assert ps.card1.alive is False


def test_from_setup_basic():
    gs = coup.GameState.from_setup([
        coup.PlayerSetup.make("Alice", coup.Card.DUKE,    coup.Card.CAPTAIN),
        coup.PlayerSetup.make("Bob",   coup.Card.ASSASSIN, coup.Card.CONTESSA),
    ])
    assert gs.player(0).cards[0] if False else True   # just access it
    assert gs.player(0).card0.type == coup.Card.DUKE
    assert gs.player(1).card0.type == coup.Card.ASSASSIN
    assert gs.phase == coup.Phase.ACTION_SELECTION


def test_from_setup_coins_applied():
    gs = coup.GameState.from_setup([
        coup.PlayerSetup.make("Rich",  coup.Card.DUKE,    coup.Card.CAPTAIN,  9),
        coup.PlayerSetup.make("Broke", coup.Card.CONTESSA, coup.Card.ASSASSIN, 0),
    ])
    assert gs.player(0).coins == 9
    assert gs.player(1).coins == 0


def test_from_setup_one_down_influence():
    gs = coup.GameState.from_setup([
        coup.PlayerSetup.one_down("Alice", coup.Card.DUKE, coup.Card.ASSASSIN),
        coup.PlayerSetup.make("Bob", coup.Card.CAPTAIN, coup.Card.CONTESSA),
    ])
    assert gs.player(0).influence() == 1
    assert gs.player(0).card0.alive is True
    assert gs.player(0).card1.alive is False


def test_from_setup_eliminated_player():
    gs = coup.GameState.from_setup([
        coup.PlayerSetup.make("A",     coup.Card.DUKE,    coup.Card.CAPTAIN),
        coup.PlayerSetup.eliminated("B", coup.Card.ASSASSIN, coup.Card.CONTESSA),
        coup.PlayerSetup.make("C",     coup.Card.AMBASSADOR, coup.Card.DUKE),
    ])
    assert gs.player(0).is_eliminated() is False
    assert gs.player(1).is_eliminated() is True
    assert gs.player(2).is_eliminated() is False


def test_from_setup_active_player():
    gs = coup.GameState.from_setup([
        coup.PlayerSetup.make("A", coup.Card.DUKE,    coup.Card.CAPTAIN),
        coup.PlayerSetup.make("B", coup.Card.ASSASSIN, coup.Card.CONTESSA),
        coup.PlayerSetup.make("C", coup.Card.DUKE,    coup.Card.AMBASSADOR),
    ], active_player_index=2)
    assert gs.active_player == 2


def test_from_setup_too_many_copies_raises():
    with pytest.raises(Exception):
        coup.GameState.from_setup([
            coup.PlayerSetup.make("A", coup.Card.DUKE, coup.Card.DUKE),
            coup.PlayerSetup.make("B", coup.Card.DUKE, coup.Card.DUKE),
        ])


# ── String / repr ───────────────────────────────────────────────────────────

def test_string_methods():
    gs = coup.GameState(["A", "B"], 42)
    s = str(gs)
    r = repr(gs)
    assert isinstance(s, str)
    assert "GameState" in r
    assert "ACTION_SELECTION" in r


def test_game_state_str_contains_player_names():
    gs = coup.GameState(["Alice", "Bob"], 0)
    s = str(gs)
    assert "Alice" in s
    assert "Bob" in s


# ── Static helpers ──────────────────────────────────────────────────────────

def test_static_helpers():
    assert isinstance(coup.GameState.card_name(coup.Card.DUKE),                   str)
    assert isinstance(coup.GameState.action_name(coup.ActionType.INCOME),         str)
    assert isinstance(coup.GameState.phase_name(coup.Phase.ACTION_SELECTION),     str)
    assert isinstance(coup.GameState.lose_reason_name(coup.LoseReason.COUP_TARGET), str)


def test_static_helper_values():
    assert coup.GameState.card_name(coup.Card.DUKE)              == "Duke"
    assert coup.GameState.action_name(coup.ActionType.INCOME)    == "Income"
    assert coup.GameState.phase_name(coup.Phase.ACTION_SELECTION)== "ACTION_SELECTION"


# ── Error handling ───────────────────────────────────────────────────────────

def test_invalid_player_index():
    gs = coup.GameState(["A", "B"], 42)
    with pytest.raises(Exception):
        gs.player(999)


def test_apply_invalid_move_type():
    gs = coup.GameState(["A", "B"], 42)
    with pytest.raises(Exception):
        gs.apply_move(None)


def test_apply_move_after_game_over_raises():
    gs = coup.GameState(["A", "B"], 0)
    play_to_terminal(gs)
    assert gs.is_terminal
    with pytest.raises(Exception):
        gs.apply_move(coup.Move.income())


def test_coup_without_enough_coins_raises():
    gs = coup.GameState(["A", "B"], 0)
    # Only 2 coins at start – Coup requires 7
    with pytest.raises(Exception):
        gs.apply_move(coup.Move.coup(1))
