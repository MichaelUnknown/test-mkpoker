/*

mkpoker - simulation of 10k hands with different agents

Copyright (C) 2021 Michael Knörzer

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

#include <mkpoker/base/range.hpp>
#include <mkpoker/game/game.hpp>
#include <mkpoker/util/array.hpp>
#include <mkpoker/util/card_generator.hpp>

#include <algorithm>    // rotate, find
#include <chrono>       // ms
#include <cstdlib>      // abs
#include <limits>       // numeric_limits<int>::max
#include <random>       //
#include <stdexcept>    // runtime_error
#include <string>       //
#include <thread>       // sleep 1s
#include <utility>      // tuple, pair

#include <fmt/core.h>
#include <fmt/format.h>
#include <sqlite/sqlite3.h>

std::vector<mkp::hand_2r> ranking;

template <>
struct fmt::formatter<mkp::hand_2r>
{
    constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin())
    {
        // Parse the presentation format and store it in the formatter:
        auto it = ctx.begin(), end = ctx.end();

        // Check if reached the end of the range:
        if (it != end && *it != '}')
            throw fmt::format_error("invalid format");

        // Return an iterator past the end of the parsed range:
        return it;
    }

    // Formats the custom type
    // stored in this formatter.
    template <typename FormatContext>
    auto format(const mkp::hand_2r& h2r, FormatContext& ctx) -> decltype(ctx.out())
    {
        // ctx.out() is an output iterator to write to.
        return format_to(ctx.out(), "{}", h2r.str());
    }
};

[[nodiscard]] auto has_joined_the_pot(const mkp::gb_playerstate_t& pstate) -> bool
{
    return (pstate == mkp::gb_playerstate_t::ALIVE || pstate == mkp::gb_playerstate_t::ALLIN);
}

class range_bot_6p
{
    // rfi percentages for positions UTG to SB
    inline static const std::array<std::pair<mkp::gb_pos_t, float>, 5> ranges_01_open_raise = {{{mkp::gb_pos_t::UTG, 12.0f},
                                                                                                {mkp::gb_pos_t::MP, 16.0f},
                                                                                                {mkp::gb_pos_t::CO, 22.0f},
                                                                                                {mkp::gb_pos_t::BTN, 30.0f},
                                                                                                {mkp::gb_pos_t::SB, 48.0f}}};
    // 4+5+0+1+2+3 raising percentages facing a raise for positions SB, BB, UTG, MP, CO, BTN
    inline static const std::array<std::vector<std::pair<mkp::gb_pos_t, float>>, 6> ranges_02_facing_raise{
        {// we are position  SB (0)
         {{mkp::gb_pos_t::UTG, 3.0f}, {mkp::gb_pos_t::MP, 5.5f}, {mkp::gb_pos_t::CO, 8.0f}, {mkp::gb_pos_t::BTN, 10.5f}},
         // we are position  BB (1)
         {{mkp::gb_pos_t::UTG, 3.0f},
          {mkp::gb_pos_t::MP, 5.5f},
          {mkp::gb_pos_t::CO, 8.0f},
          {mkp::gb_pos_t::BTN, 10.5f},
          {mkp::gb_pos_t::SB, 13.0f}},
         // we are position UTG (2)
         {},
         // we are position  MP (3)
         {{mkp::gb_pos_t::UTG, 3.0f}},
         // we are position  CO (4)
         {{mkp::gb_pos_t::UTG, 4.0f}, {mkp::gb_pos_t::MP, 5.0f}},
         // we are position BTN (5)
         {{mkp::gb_pos_t::UTG, 6.0f}, {mkp::gb_pos_t::MP, 7.0f}, {mkp::gb_pos_t::CO, 8.0f}}}};

    [[nodiscard]] auto cards_in_percentage(const mkp::hand_2c& cards, const float percentage) -> bool
    {
        const auto absolute = 169.0f * percentage / 100;
        const auto it_start = ranking.cbegin();
        const auto it_end = it_start + static_cast<int>(absolute);
        const auto hand = mkp::range::hand(mkp::range::index(cards));
        const auto find_it = std::find(it_start, it_end, hand);
        if (find_it != it_end)
        {
            return true;
        }
        else
        {
            return false;
        }
    }

    [[nodiscard]] auto try_raise_pot(const mkp::gamestate<6>& game) const -> mkp::player_action_t
    {
        // try to raise as close to 1.0 pot as possible
        auto all_actions = game.possible_actions();
        const auto pot_sized_raise = 2 * game.amount_to_call() + game.pot_size();
        auto distance_to_pot_raise = [&pot_sized_raise](const mkp::player_action_t a) -> int {
            // return distance to action 'raise pot size'
            if (a.m_action == mkp::gb_action_t::FOLD)
            {
                return std::numeric_limits<int>::max();    // worst case
            }
            else if (a.m_action == mkp::gb_action_t::CHECK)
            {
                return std::numeric_limits<int>::max() - 1;    // not much better
            }
            else
            {
                return std::abs(a.m_amount - pot_sized_raise);
            }
        };
        std::sort(all_actions.begin(), all_actions.end(),
                  [&](const auto& lhs, const auto& rhs) { return distance_to_pot_raise(lhs) < distance_to_pot_raise(rhs); });
        return all_actions.front();
    };

    [[nodiscard]] auto action_preflop(const mkp::gamestate<6>& game, const mkp::hand_2c& cards) -> mkp::player_action_t
    {
        const auto all_state = game.all_players_state();
        const auto player_joined_pot = std::find_if(all_state.cbegin(), all_state.cend(), has_joined_the_pot);

        if (player_joined_pot == all_state.cend() /* is open raise */)
        {
            // get rfi percentage from range and check if hand is in range
            const auto it = std::find_if(ranges_01_open_raise.cbegin(), ranges_01_open_raise.cend(),
                                         [&](const auto& pair) { return pair.first == game.active_player_v(); });
            if (cards_in_percentage(cards, it->second))
            {
                fmt::print(" // raised hand {}, in top {:05.2f}% of hands // ", cards.str(), it->second);
                return try_raise_pot(game);
            }
            else
            {
                fmt::print(" // folded hand {}, not in top {:05.2f}% of hands // ", cards.str(), it->second);
                return mkp::player_action_t{0, mkp::gb_action_t::FOLD, game.active_player_v()};
            }
            //const auto absolute = 169.0f * it->second / 100;
            //const auto it_start = ranking.cbegin();
            //const auto it_end = it_start + static_cast<int>(absolute);
            //const auto hand = mkp::range::hand(mkp::range::index(cards));
            ////const auto lowest_hands = std::vector(it_end - 6, it_end);
            ////fmt::print("raising {}% of hands ({}/169) // lowest hands are: {}\n", it->second, absolute, fmt::join(lowest_hands, ", "));
            //const auto find_it = std::find(it_start, it_end, hand);
            //if (find_it != it_end)
            //{
            //    // try raise pot
            //    fmt::print(" // raised hand {}, in top {:05.2f}% of hands // ", cards.str(), it->second);
            //    return try_raise_pot(game);
            //}
            //else
            //{
            //    // fold
            //    fmt::print(" // folded hand {}, not in top {:05.2f}% of hands // ", cards.str(), it->second);
            //    return mkp::player_action_t{0, mkp::gb_action_t::FOLD, game.active_player_v()};
            //}
        }
        else
        {
            // pot already opened
            const auto players_in_pot = std::count_if(all_state.cbegin(), all_state.cend(), has_joined_the_pot);

            if (players_in_pot == 1)
            {
                // 1v1 -> get raise percentage from range and check if hand is in range
                const auto my_pos = game.active_player();
                const auto op_pos_it = std::find_if(all_state.cbegin(), all_state.cend(), has_joined_the_pot);
                const auto op_pos = static_cast<mkp::gb_pos_t>(std::distance(all_state.cbegin(), op_pos_it));
                const auto it = std::find_if(ranges_02_facing_raise[my_pos].cbegin(), ranges_02_facing_raise[my_pos].cend(),
                                             [&](const auto& pair) { return pair.first == op_pos; });
                if (cards_in_percentage(cards, it->second))
                {
                    fmt::print(" // raised hand {}, in top {:05.2f}% of hands // ", cards.str(), it->second);
                    return try_raise_pot(game);
                }
                else
                {
                    fmt::print(" // folded hand {}, not in top {:05.2f}% of hands // ", cards.str(), it->second);
                    return mkp::player_action_t{0, mkp::gb_action_t::FOLD, game.active_player_v()};
                }
                //const auto absolute = 169.0f * it->second / 100;
                //const auto it_start = ranking.cbegin();
                //const auto it_end = it_start + static_cast<int>(absolute);
                //const auto hand = mkp::range::hand(mkp::range::index(cards));
                //const auto find_it = std::find(it_start, it_end, hand);
                //if (find_it != it_end)
                //{
                //    // try raise pot
                //    fmt::print(" // raised hand {}, in top {:05.2f}% of hands // ", cards.str(), it->second);
                //    return try_raise_pot(game);
                //}
                //else
                //{
                //    // fold
                //    fmt::print(" // folded hand {}, not in top {:05.2f}% of hands // ", cards.str(), it->second);
                //    return mkp::player_action_t{0, mkp::gb_action_t::FOLD, game.active_player_v()};
                //}
            }
            else
            {
                // multiway
                ;
            }

            return game.possible_actions().front();
        }
    };

    [[nodiscard]] auto action_rest(const mkp::gamestate<6>& game) -> mkp::player_action_t
    {
        // return first possible move
        return game.possible_actions().front();
    };

   public:
    [[nodiscard]] auto do_action(const mkp::gamestate<6>& game, const mkp::hand_2c& cards) -> mkp::player_action_t
    {
        switch (game.gamestate_v())
        {
            case mkp::gb_gamestate_t::PREFLOP_BET:
                return action_preflop(game, cards);
                break;
            default:
                return action_rest(game);
                break;
        }
    }
};

//const std::array<std::pair<mkp::gb_pos_t, float>, 5> range_bot_6p::ranges_01_open_raise = {{{mkp::gb_pos_t::UTG, 12.0f},
//                                                                                            {mkp::gb_pos_t::MP, 16.0f},
//                                                                                            {mkp::gb_pos_t::CO, 22.0f},
//                                                                                            {mkp::gb_pos_t::BTN, 30.0f},
//                                                                                            {mkp::gb_pos_t::SB, 48.0f}}};

auto action_6p_fold_bot(const mkp::gamestate<6>& game, const mkp::hand_2c& cards)
{
    return mkp::player_action_t{0, mkp::gb_action_t::FOLD, game.active_player_v()};
}

int main()
{
    // make db and store results
    sqlite3* db;
    char* zErrMsg = 0;
    int rc;

    auto check_answer_and_print = [&](const auto msg) {
        if (rc != SQLITE_OK)
        {
            fmt::print("SQL error: {}\n", zErrMsg);
            sqlite3_free(zErrMsg);
        }
        else
        {
            fmt::print(msg);
        }
    };
    // create / open db
    rc = sqlite3_open("../../../../data/river_results.db", &db);
    if (rc)
    {
        fmt::print("Can't open database: {}\n", sqlite3_errmsg(db));
        return EXIT_FAILURE;
    }
    else
    {
        fmt::print("Opened database successfully\n");
    }

    const auto sql_read = "SELECT HAND FROM RIVERS_AFTER_V1;";
    rc = sqlite3_exec(
        db, sql_read,
        [](void* arg_vec, int argc, char** argv, char** azColName) -> int {
            // fill rankings
            std::vector<mkp::hand_2r>* vec = static_cast<std::vector<mkp::hand_2r>*>(arg_vec);
            int i;
            for (i = 0; i < argc; i++)
            {
                const std::string str{argv[i]};
                const auto h2r = mkp::hand_2r(str.substr(0, 2));
                vec->push_back(h2r);
                //fmt::print("{} <=> {}\n", argv[i], h2r.str());
            }
            return 0;
        },
        static_cast<void*>(&ranking), &zErrMsg);
    check_answer_and_print("ranking sucessfully read from db\n");

    constexpr std::size_t c_num_players = 6;
    constexpr int c_starting_chips = 100'000;
    std::string input{};
    std::mt19937 rng{1927};
    mkp::card_generator cgen{};

    // keep track of player chips
    const std::array<int, c_num_players> starting_chips{c_starting_chips, c_starting_chips, c_starting_chips,
                                                        c_starting_chips, c_starting_chips, c_starting_chips};
    //std::array<int, c_num_players> chips = starting_chips;
    //std::array<int, c_num_players> last_round = starting_chips;
    //std::array<std::string, c_num_players> names{"Alex", "Bert", "Charles", "Dave", "Emely", "Frank"};
    //std::array<int, c_num_players> names_pos{0, 1, 2, 3, 4, 5};

    struct name_id_t
    {
        std::string m_name;
        uint8_t m_id;
    };
    struct chips_id_t
    {
        int32_t m_chips;
        uint8_t m_id;
    };
    std::array<name_id_t, c_num_players> players_info{{{"Alex", 0}, {"Bert", 1}, {"Charles", 2}, {"Dave", 3}, {"Emely", 4}, {"Frank", 5}}};
    std::array<chips_id_t, c_num_players> players_chips = {{{starting_chips[0], players_info[0].m_id},
                                                            {starting_chips[1], players_info[1].m_id},
                                                            {starting_chips[2], players_info[2].m_id},
                                                            {starting_chips[3], players_info[3].m_id},
                                                            {starting_chips[4], players_info[4].m_id},
                                                            {starting_chips[5], players_info[5].m_id}}};
    //auto last_round = players_chips;

    const int c_num_name_length = 7;
    const int c_num_number_display_digits = 9;
    int counter = 0;

    fmt::print("mkpoker - simulation of 10k hands with different agents\nTo exit the program, type 'Ctrl + c' at any time\n\n");

    range_bot_6p range_bot{};

    for (uint8_t player_pos = 0;; player_pos = (player_pos + 1) % c_num_players)
    {
        // generate game/cards for a six player game
        const auto random_cards = cgen.generate_v(5 + 2 * c_num_players);
        const mkp::gamecards<c_num_players> gamecards(random_cards);
        auto game = mkp::gamestate<c_num_players>(starting_chips);
        bool print_gamestate = true;
        //        fmt::print( "New hand started. Your position is " << std::to_string(player_pos) << " \n";

        for (;;)
        {
#if !defined(NDEBUG)
            if (print_gamestate)
            {
                fmt::print("{}", game.str_state());
            }
            //fmt::print("The active player is: {}{}\n", std::to_string(game.active_player()),
            //           game.active_player() == player_pos ? " (this is you)" : "");
#endif

            // find which player is active, get ID from players_chips
            const auto player_id = players_chips[game.active_player()].m_id;
            //const auto it = std::find(names_pos.cbegin(), names_pos.cend(), game.active_player());
            //const auto idx = std::distance(names_pos.cbegin(), it);
            const auto action = [&]() {
                if (player_id > 1)
                {
                    fmt::print("It is {}'s turn in position {} (ID {}) playing 'always fold'", players_info[player_id].m_name,
                               game.active_player(), player_id);
                    return action_6p_fold_bot(game, gamecards.m_hands[game.active_player()]);
                }
                else
                {
                    fmt::print("It is {}'s turn in position {} (ID {}) playing 'range algorithm'", players_info[player_id].m_name,
                               game.active_player(), player_id);
                    //fmt::print("It is {}'s turn in position {} (index {}) playing 'range algorithm'", names[idx], game.active_player(),
                    //           idx);
                    return range_bot.do_action(game, gamecards.m_hands[game.active_player()]);
                }
            }();

#if !defined(NDEBUG)
            fmt::print(" -> {}\n", action.str());
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
#else
            fmt::print("\n", action.str());
#endif
            if (action.m_action == mkp::gb_action_t::FOLD || action.m_action == mkp::gb_action_t::CHECK)
            {
                print_gamestate = false;
            }
            else
            {
                print_gamestate = true;
            }
            game.execute_action(action);

            // check if hand/round finished
            if (game.in_terminal_state())
            {
#if !defined(NDEBUG)
                fmt::print("The hand ended.\n{}", game.str_state());

                const auto pots = game.all_pots();
                for (unsigned int i = 1; auto&& e : pots)
                {
                    fmt::print("Pot {} :\nEligible players: ", i);
                    for (auto&& p : std::get<0>(e))
                    {
                        fmt::print("{} ({}) ", p, gamecards.m_hands[p].str());
                    }
                    fmt::print("\nThe board is: ");
                    for (const auto cards = gamecards.board_n(5); auto&& c : cards)
                    {
                        fmt::print("{} ", c.str());
                    }
                    fmt::print("\nlower: {}, upper: {}\n", std::get<2>(e), std::get<1>(e));
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    ++i;
                }
#endif
                const auto results = game.is_showdown() ? game.payouts_showdown(gamecards) : game.payouts_noshowdown();
#if !defined(NDEBUG)
                fmt::print("Results:\n");
                for (unsigned int i = 0; i < results.size(); ++i)
                {
                    fmt::print("#{} ({:>{}}): {:>{}} => {:>{}}\n", i, players_info[players_chips[i].m_id].m_name, c_num_name_length,
                               results[i], c_num_number_display_digits, starting_chips[i] + results[i], c_num_number_display_digits);
                }
                //fmt::print("\n");
#endif

                //last_round = players_chips;
                for (unsigned int i = 0; i < players_chips.size(); ++i)
                {
                    players_chips[i].m_chips += results[i];
                }

                constexpr int print_intervall = 5;
                ++counter;
                if (counter % print_intervall == 0)
                {
                    fmt::print("Total after {} hands:\n", counter);
                    for (unsigned int i = 0; i < players_info.size(); ++i)
                    {
                        const auto it =
                            std::find_if(players_chips.cbegin(), players_chips.cend(), [&](const auto& e) { return e.m_id == i; });
                        //const auto test = it_id->m_id;
                        fmt::print("#{} ({:>{}}): {:>{}}\n", it->m_id, players_info[it->m_id].m_name, c_num_name_length, it->m_chips,
                                   c_num_number_display_digits);
                    }

                    //fmt::print( "Last round:\n";
                    //for (unsigned int i = 0; i < results.size(); ++i)
                    //{
                    //    fmt::print("#{} ({:>{}}): {:>{}}\n", i, names[i], c_num_name_length, last_round[i], c_num_number_display_digits);
                    //}
                }
                fmt::print("\n");

                //std::this_thread::sleep_for(std::chrono::milliseconds(50));

                // each player moves up one seat the next round
                std::rotate(players_chips.rbegin(), players_chips.rbegin() + 1, players_chips.rend());
                //std::rotate(names_pos.rbegin(), names_pos.rbegin() + 1, names_pos.rend());

                break;
            }
        }
    }
}