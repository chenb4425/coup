"""
example.py - Demonstrates using the coup_py Python bindings.

Build first:
    pip install pybind11
    mkdir build && cd build
    cmake -DBUILD_PYTHON_BINDINGS=ON ..
    make -j$(nproc)
    # The .so lands at  build/coup_py*.so
    # Run from repo root: PYTHONPATH=build python example.py
"""

import sys
import random

try:
    import coup_py as coup
except ImportError:
    print("coup_py not found - build the bindings first (see README).")
    sys.exit(1)


# ── 1. Basic game setup ───────────────────────────────────────────────────────

print("=== Coup Python Example ===\n")

gs = coup.GameState(["Alice", "Bob", "Charlie"], seed=42)
print(gs)


# ── 3. Play one full game with random legal moves ──────────────────────────────

rng = random.Random()

step = 0
while not gs.is_terminal:
    moves = gs.legal_moves()
    print("current possible moves : ", moves)
    assert moves, "legal_moves() must never be empty"

    # Show what the active / responding player is about to do
    phase = coup.GameState.phase_name(gs.phase)
    responding = gs.responding_player
    active = gs.active_player

    chosen = rng.choice(moves)
    actor_idx = responding if responding >= 0 else active
    actor_name = gs.player(actor_idx).name

    print(f"Step {step:>3}  [{phase}]  "
          f"{actor_name} → {coup.GameState.action_name(chosen.action)}"
          + (f"  target={gs.player(chosen.target).name}"
             if chosen.target >= 0 else "")
          + (f"  card={coup.GameState.card_name(chosen.card)}"
             if chosen.card != coup.Card.NONE else "")
          + (f"  idx={chosen.card_idx}" if chosen.card_idx >= 0 else ""))

    gs.apply_move(chosen)
    step += 1

# ── 4. Print final state ──────────────────────────────────────────────────────

print("\n" + str(gs))
w = gs.winner
print(f"Winner: {gs.player(w).name} (player index {w})")
print(f"Game lasted {step} move-steps.\n")



# ── 5. Show pending_action / pending_block API ────────────────────────────────

print("=== Foreign-Aid + Block demonstration ===\n")
gs2 = coup.GameState(["Dave", "Eve"], seed=7)

# Force Dave to try Foreign Aid
gs2.apply_move(coup.Move.foreign_aid())
print(f"Phase after Foreign Aid: {coup.GameState.phase_name(gs2.phase)}")
pa = gs2.pending_action
print(f"Pending action: {coup.GameState.action_name(pa.action)}  "
      f"challengeable={pa.challengeable}  blockable={pa.blockable}")

# Eve blocks with Duke
gs2.apply_move(coup.Move.block(coup.Card.DUKE))
print(f"Phase after block: {coup.GameState.phase_name(gs2.phase)}")
pb = gs2.pending_block
if pb is not None:
    print(f"Pending block: {gs2.player(pb.blocker).name} claims "
          f"{coup.GameState.card_name(pb.claimed_card)}")

# Dave passes (accepts the block)
gs2.apply_move(coup.Move.pass_())
print(f"Phase after Dave passes: {coup.GameState.phase_name(gs2.phase)}")
print(f"Active player now: {gs2.player(gs2.active_player).name}")
print(f"Dave coins: {gs2.player(0).coins}  (should still be 2 - FA blocked)\n")

# ── 6. Lose Influence demo ────────────────────────────────────────────────────

print("=== Coup + Lose Influence demo ===\n")
gs3 = coup.GameState(["Frank", "Grace"], seed=0)

# Give Frank 7 coins by choosing income repeatedly
# (each income is 1 coin; we need 5 more = 5 turns of income for Frank)
for _ in range(10):
    moves = gs3.legal_moves()
    best = next((m for m in moves if m.action == coup.ActionType.INCOME), moves[0])
    gs3.apply_move(best)
    if gs3.player(0).coins >= 7:
        break

print(f"Frank coins: {gs3.player(0).coins}")
if gs3.player(0).coins >= 7 and gs3.phase == coup.Phase.ACTION_SELECTION \
        and gs3.active_player == 0:
    gs3.apply_move(coup.Move.coup(1))   # Frank coups Grace
    print(f"Phase: {coup.GameState.phase_name(gs3.phase)}")
    print(f"Lose-influence player: {gs3.player(gs3.lose_influence_player).name}")
    print(f"Reason: {coup.GameState.lose_reason_name(gs3.lose_reason)}")
    print(f"Legal moves: {[m for m in gs3.legal_moves()]}")
    gs3.apply_move(coup.Move.lose_card(0))  # Grace loses card 0
    print(f"Grace influence after: {gs3.player(1).influence()}")



