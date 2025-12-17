import game_env
import random


#init GameState struct
game = game_env.GameState()

num_players = 5

#Players and their cards,
#   0 implies no card
#   1-5 implies 
#   1 : duke
#   2 : captain
#   3 : Ambassador
#   4 : Assasin
#   5 : Contessa
#dont change this unless you also change game_logic variables
values = [1,2,3,4,5]
num_each_card = 3
deck = values * num_each_card

random.shuffle(deck)

card_1 = deck[:num_players]
card_2 = deck[num_players:2*num_players]
deck = deck[2*num_players:]

#card1 is an array with the players first card
#card2 is an array with playeys second card
game_env.reset(game, num_players, deck, card_1, card_2)




#############  MAIN GAME LOOP  #############
while game_env.get_players(game) > 1:
    
    current_turn = game_env.get_turn(game)
    possible_actions = game_env.get_actions(game)
    my_money = game_env.get_money(game)
    my_cards = game_env.get_current_turn_cards(game)
    shown_cards = game_env.get_cards_shown(game)

    """
    Its probably also important to add a variable that records all previous turns from players
    So then our model can be trained on lots of the other players information
    """


    #Then our model will choose what path to take
    game_env.take_turn(game, <action>)





"""
STEPS PYTHON WILL TAKE

1)  We init a game in python by randomly selecting cards, and then by calling reset with cards parameter

2)  We ask our game_logic, whos turn is it right now, and what actions can they do?
        - We give other usefull information for training such as what card do I have?
        - What cards have already been shown,
        - How much money do i have, and how much money does other people have?

3)  That person goes and then we repeat this cycle,

These are the only steps we want python to take,
Since we will have a number of models running a the same time its important
That we just ask whos turn is it, and the ml reponsible for that turn will go

"""