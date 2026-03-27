import re
from enum import IntEnum

class Card(IntEnum):
    DUKE = 0
    ASSASSIN = 1
    CAPTAIN = 2
    AMBASSADOR = 3
    CONTESSA = 4
    NONE = 255

class ActionType(IntEnum):
    INCOME = 0
    FOREIGN_AID = 1
    COUP = 2
    TAX = 3
    ASSASSINATE = 4
    STEAL = 5
    EXCHANGE = 6
    PASS = 7
    CHALLENGE = 8
    BLOCK = 9
    LOSE_CARD = 10
    KEEP_CARDS = 11


def parse_moves(s):

    pattern = r"<Move (\w+)(.*?)>"

    moves = []

    for action, rest in re.findall(pattern, s):
        attrs = {}
        
        if rest:
            # find key=value pairs
            pairs = re.findall(r"(\w+)=([\w\d]+)", rest)
            for k, v in pairs:
                attrs[k] = v
        
        moves.append({
            "action": action,
            **attrs
        })

    return moves