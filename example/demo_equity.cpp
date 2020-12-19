/*

mkpoker - demo command line app that prints the equities of random hands to the console

Copyright (C) 2020 Michael Knörzer

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as
published by the Free Software Foundation, either version 3 of the
License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include <mkpoker/base/card.hpp>
#include <mkpoker/base/hand.hpp>
#include <mkpoker/holdem/holdem_evaluation.hpp>
#include <mkpoker/util/bit.hpp>
#include <mkpoker/util/card_generator.hpp>

#include <array>
#include <iostream>
#include <random>
#include <span>

int main()
{
    mkp::card_generator cgen{std::random_device{}()};

    const auto cards = cgen.generate_v(9);
    const auto h1 = mkp::hand_2c(cards[0], cards[1]);
    const auto h2 = mkp::hand_2c(cards[2], cards[3]);

    // print hands
    std::cout << "randomly generated game:\nhand 1: " << h1.str() << "\nhand 2: " << h2.str() << "\n";

    // preflop eval: there are (48 choose 5) = 1'712'304 possible outcomes
    // multiply wins by 2; draws are 1
    std::array<int, 2> wins_preflop{0, 0};
    for (uint8_t i = 0; i < mkp::c_deck_size; ++i)
    {
        for (uint8_t j = i + 1; j < mkp::c_deck_size; ++j)
        {
            for (uint8_t k = j + 1; k < mkp::c_deck_size; ++k)
            {
                for (uint8_t l = k + 1; l < mkp::c_deck_size; ++l)
                {
                    for (uint8_t m = l + 1; m < mkp::c_deck_size; ++m)
                    {
                        const mkp::cardset board{mkp::make_bitset(i, j, k, l, m)};
                        const auto e1 = mkp::evaluate_safe(h1.as_cardset(), board);
                        const auto e2 = mkp::evaluate_safe(h2.as_cardset(), board);

                        if (const auto cmp = e1 <=> e2; cmp == 0)
                        {
                            wins_preflop[0] += 1;
                            wins_preflop[1] += 1;
                        }
                        else
                        {
                            if (cmp > 0)
                            {
                                wins_preflop[0] += 2;
                            }
                            else
                            {
                                wins_preflop[1] += 2;
                            }
                        }
                    }
                }
            }
        }
    }
    // print equity
    std::cout << "\npreflop:\nequity hand 1: " << float(wins_preflop[0]) / (wins_preflop[0] + wins_preflop[1]) * 100.0;
    std::cout << "\nequity hand 2: " << float(wins_preflop[1]) / (wins_preflop[0] + wins_preflop[1]) * 100.0;
    std::cout << "\n";

    // print flop
    const auto board_flop = mkp::cardset{std::span(cards.data() + 4, 3)};
    const auto all_cards_flop = mkp::cardset{std::span(cards.data(), 7)};
    std::cout << "\nboard after flop: " << board_flop.str() << "\n";

    // flop eval: there are (45 choose 2) = 990 possible outcomes
    // multiply wins by 2; draws are 1
    std::array<int, 2> wins_flop{0, 0};
    for (uint8_t i = 0; i < mkp::c_deck_size; ++i)
    {
        const auto ci = mkp::card{i};
        if (all_cards_flop.contains(ci))
        {
            continue;
        }

        for (uint8_t j = i + 1; j < mkp::c_deck_size; ++j)
        {
            const auto cj = mkp::card{j};
            if (all_cards_flop.contains(cj))
            {
                continue;
            }

            const auto e1 = mkp::evaluate_safe(board_flop, h1.as_cardset(), ci, cj);
            const auto e2 = mkp::evaluate_safe(board_flop, h2.as_cardset(), ci, cj);

            if (const auto cmp = e1 <=> e2; cmp == 0)
            {
                wins_flop[0] += 1;
                wins_flop[1] += 1;
            }
            else
            {
                if (cmp > 0)
                {
                    wins_flop[0] += 2;
                }
                else
                {
                    wins_flop[1] += 2;
                }
            }
        }
    }
    // print equity
    std::cout << "equity hand 1: " << float(wins_flop[0]) / (wins_flop[0] + wins_flop[1]) * 100.0;
    std::cout << "\nequity hand 2: " << float(wins_flop[1]) / (wins_flop[0] + wins_flop[1]) * 100.0;
    std::cout << "\n";

    // print turn
    const auto board_turn = mkp::cardset{std::span(cards.data() + 4, 4)};
    const auto all_cards_turn = mkp::cardset{std::span(cards.data(), 8)};
    std::cout << "\nboard after turn: " << board_turn.str() << "\n";

    // turn eval: there are 44 possible outcomes
    std::array<int, 2> wins_turn{0, 0};
    for (uint8_t i = 0; i < mkp::c_deck_size; ++i)
    {
        const auto ci = mkp::card{i};
        if (all_cards_turn.contains(ci))
        {
            continue;
        }

        const auto e1 = mkp::evaluate_safe(board_turn, h1.as_cardset(), ci);
        const auto e2 = mkp::evaluate_safe(board_turn, h2.as_cardset(), ci);

        if (const auto cmp = e1 <=> e2; cmp == 0)
        {
            wins_turn[0] += 1;
            wins_turn[1] += 1;
        }
        else
        {
            if (cmp > 0)
            {
                wins_turn[0] += 2;
            }
            else
            {
                wins_turn[1] += 2;
            }
        }
    }
    // print equity
    std::cout << "equity hand 1: " << float(wins_turn[0]) / (wins_turn[0] + wins_turn[1]) * 100.0;
    std::cout << "\nequity hand 2: " << float(wins_turn[1]) / (wins_turn[0] + wins_turn[1]) * 100.0;
    std::cout << "\n";

    // print river
    const auto board_river = mkp::cardset{std::span(cards.data() + 4, 5)};
    const auto all_cards_river = mkp::cardset{cards};
    std::cout << "\nboard after river: " << board_river.str() << "\n";

    const auto e1 = mkp::evaluate_safe(board_river, h1.as_cardset());
    const auto e2 = mkp::evaluate_safe(board_river, h2.as_cardset());

    if (const auto cmp = e1 <=> e2; cmp == 0)
    {
        std::cout << "equity hand 1: " << 50;
        std::cout << "\nequity hand 2: " << 50;
    }
    else
    {
        if (cmp > 0)
        {
            std::cout << "equity hand 1: " << 100;
            std::cout << "\nequity hand 2: " << 0;
        }
        else
        {
            std::cout << "equity hand 1: " << 0;
            std::cout << "\nequity hand 2: " << 100;
        }
    }
    std::cout << "\n\n";

    return EXIT_SUCCESS;
}