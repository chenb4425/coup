# Coup C++ Game State Engine

A complete, self-contained C++ implementation of the [Coup](https://en.wikipedia.org/wiki/Coup_(card_game)) card game state, with pybind11 Python bindings.  Designed as a clean foundation for AI agents, MCTS, reinforcement-learning environments, or human-facing UIs.

---

## Project layout

```
coup_cpp/
├── CMakeLists.txt
├── README.md
├── example.py                  ← Python usage demo
├── include/
│   └── coup.hpp                ← Full public API (header-only interface)
├── include/
│   └── coup_test.cpp
└── src/
    ├── coup.cpp                ← Game-state implementation
    ├── bindings.cpp            ← pybind11 Python bindings
    └── main_test.cpp           ← C++ unit + simulation tests
```

---

## Building

### C++ only (no Python)

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
ctest          # run all tests
```

### With Python bindings

```bash
pip install pybind11
mkdir build && cd build
cmake -DBUILD_PYTHON_BINDINGS=ON ..
make -j$(nproc)
# The shared library lands at  build/coup_py*.so
cd ..
PYTHONPATH=build python example.py
```

---

## State-machine overview

```
ACTION_SELECTION
    │  actor picks Income / Foreign Aid / Coup / Tax / Assassinate / Steal / Exchange
    ▼
AWAITING_ACTION_RESPONSE          (one player at a time, clockwise)
    │  each non-actor can: PASS | CHALLENGE | BLOCK
    │
    ├─ all PASS ──────────────────────────────────► resolve action
    │
    ├─ CHALLENGE:
    │     actor HAS card  → proveCard(); challenger enters LOSE_INFLUENCE
    │                        → then resolve action
    │     actor LACKS card → actor enters LOSE_INFLUENCE → action fails
    │
    └─ BLOCK (claims blocking card):
         ▼
    AWAITING_BLOCK_RESPONSE       (everyone except blocker, starting from actor)
         │  each can: PASS | CHALLENGE
         │
         ├─ all PASS ─────────────────────────────► action fails (block stands)
         │
         ├─ CHALLENGE:
               blocker HAS card → proveCard(); challenger LOSE_INFLUENCE
                                   → action fails (block stands)
               blocker LACKS    → blocker LOSE_INFLUENCE
                                   → resolve action


LOSE_INFLUENCE
    │  designated player reveals one of their two cards
    ▼  then: check game-over / resolve action / advance turn

EXCHANGE_SELECTION
    │  actor holds (own live cards + 2 drawn); picks which to keep (bitmask)
    ▼  unchosen cards return to court deck; then advance turn

GAME_OVER
```

---

## C++ API quick-reference

```cpp
#include "coup.hpp"
using namespace coup;

// Construct
GameState gs({"Alice", "Bob", "Charlie"}, /*seed=*/42);

// The main loop
while (!gs.isTerminal()) {
    std::vector<Move> moves = gs.legalMoves();

    // … pick a move (random, MCTS, human input, …) …
    gs.applyMove(moves[0]);
}

int winner = gs.winner();  // 0-indexed player index
```

### Key types

| Type           | Purpose                                                                                      |
| -------------- | -------------------------------------------------------------------------------------------- |
| `GameState`  | Entire mutable game state                                                                    |
| `Move`       | A move; factory methods:`Move::income()`, `Move::coup(tgt)`, `Move::block(card)`, etc. |
| `Phase`      | Current state-machine phase                                                                  |
| `ActionType` | Every possible action/response type                                                          |
| `Card`       | Duke / Assassin / Captain / Ambassador / Contessa                                            |
| `LoseReason` | Why a player is currently losing an influence                                                |
| `Player`     | Name, coins, two `InfluenceCard`s                                                          |

---

## Python API quick-reference

```python
import coup_py as coup

gs = coup.GameState(["Alice", "Bob"], seed=42)

while not gs.is_terminal:
    moves = gs.legal_moves()
    gs.apply_move(moves[0])

print(f"Winner: {gs.player(gs.winner).name}")

# Move factories
coup.Move.income()
coup.Move.foreign_aid()
coup.Move.coup(target=1)
coup.Move.tax()
coup.Move.assassinate(target=1)
coup.Move.steal(target=1)
coup.Move.exchange()
coup.Move.pass_()
coup.Move.challenge()
coup.Move.block(claimed_card=coup.Card.DUKE)
coup.Move.lose_card(card_index=0)    # 0 or 1
coup.Move.keep_cards(bitmask=0b0110) # during Exchange
```

---

## Rules implemented

| Action      | Character  | Cost    | Effect                 | Can be blocked by                  | Can be challenged |
| ----------- | ---------- | ------- | ---------------------- | ---------------------------------- | ----------------- |
| Income      | —         | —      | +1 coin                | —                                 | No                |
| Foreign Aid | —         | —      | +2 coins               | Duke                               | No                |
| Coup        | —         | 7 coins | Target loses influence | —                                 | No                |
| Tax         | Duke       | —      | +3 coins               | —                                 | Yes               |
| Assassinate | Assassin   | 3 coins | Target loses influence | Contessa (target only)             | Yes               |
| Steal       | Captain    | —      | +2 coins from target   | Captain / Ambassador (target only) | Yes               |
| Exchange    | Ambassador | —      | Swap cards with deck   | —                                 | Yes               |

**Challenge resolution:**

- Actor *proves* card → card returned to deck, new one drawn; challenger loses influence; action resolves.
- Actor *bluffs* → actor loses influence; action fails (Assassin still loses 3 coins).

**Block challenge resolution:**

- Blocker *proves* card → same swap; challenger loses influence; block stands.
- Blocker *bluffs* → blocker loses influence; original action resolves.

**Mandatory coup:** at 10+ coins you *must* coup on your turn.

---

## Notes for AI use

- `legalMoves()` always returns a non-empty list in non-terminal states.
- Imperfect information: each player only *knows* their own cards. The full state (all cards) is visible for self-play / centralized training; an agent should be given only its own player's cards in a real partial-info setting.
- Seed the RNG for reproducible games; the same seed + same move sequence always produces the same outcome.
