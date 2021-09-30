/*

mkpoker - calculates the best starting hands

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

#include <mkpoker/base/cardset.hpp>
#include <mkpoker/base/hand.hpp>
#include <mkpoker/holdem/holdem_evaluation.hpp>

#include <algorithm>
#include <set>
#include <vector>

#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ranges.h>

struct stats_t
{
    mkp::holdem_result m_score;
    uint16_t m_ranking;
};

struct hand_with_info_t
{
    mkp::hand_2c m_hand;
    uint16_t m_id;
    std::vector<stats_t> m_stats;
};
struct stats_with_id_t
{
    uint16_t m_id;
    uint16_t m_ranking;
    mkp::holdem_result m_score;
};

template <>
struct fmt::formatter<stats_t>
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
    auto format(const stats_t& st, FormatContext& ctx) -> decltype(ctx.out())
    {
        // ctx.out() is an output iterator to write to.
        return format_to(ctx.out(), "({:<34} => {:>4})", st.m_score.str(), st.m_ranking);
    }
};

template <>
struct fmt::formatter<stats_with_id_t>
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
    auto format(const stats_with_id_t& st, FormatContext& ctx) -> decltype(ctx.out())
    {
        // ctx.out() is an output iterator to write to.
        return format_to(ctx.out(), "({:>4}, {:<34} => {:>4})", st.m_id, st.m_score.str(), st.m_ranking);
    }
};

int main()
{
    //{
    //    stats_t test{mkp::evaluate_safe(mkp::cardset(mkp::make_bitset(1, 2, 3, 4, 5))), 33};
    //    fmt::print("{}\n", test);

    //    std::vector<std::string> choices = {"apple", "pear", "banana"};
    //    fmt::print("Your can choose {}!\n", fmt::join(choices, " or "));

    //    std::vector<int> vec_int{1, 2, 3, 4};
    //    fmt::print("{}\n", fmt::join(vec_int, ", "));

    //    std::vector<stats_t> vec_stats{test, test};
    //    fmt::print("{}\n", fmt::join(vec_stats, "\n"));
    //}
    {
        // sanity check
        // compute all possible cardsets with two cards in two different ways
        std::set<mkp::cardset> s_cs_hands1;
        std::set<mkp::cardset> s_cs_hands2;
        std::vector<mkp::cardset> v_cs_hands1;
        std::vector<mkp::cardset> v_cs_hands2;

        // # 1
        for (int i = 0; i < 52; ++i)
        {
            for (int j = 0; j < 52; ++j)
            {
                if (i == j)
                {
                    continue;
                }
                const auto c1 = mkp::card(i);
                const auto c2 = mkp::card(j);

                const auto cs = mkp::cardset{c1, c2};

                s_cs_hands1.insert(cs);
                v_cs_hands1.push_back(cs);
            }
        }
        // # 2
        for (int i = 0; i < 52; ++i)
        {
            for (int j = i + 1; j < 52; ++j)
            {
                if (i == j)
                {
                    fmt::print("this should never happen!\n");
                    continue;
                }
                const auto c1 = mkp::card(i);
                const auto c2 = mkp::card(j);

                const auto cs = mkp::cardset{c1, c2};

                s_cs_hands2.insert(cs);
                v_cs_hands2.push_back(cs);
            }
        }

        // compare stats
        fmt::print("size of set of hands: {} vs {}\n", s_cs_hands1.size(), s_cs_hands2.size());
        fmt::print("size of vector of hands: {} vs {}\n", v_cs_hands1.size(), v_cs_hands2.size());
    }

    {
        std::set<mkp::cardset> s_cs_flops1;
        std::set<mkp::cardset> s_cs_flops2;
        std::vector<mkp::cardset> v_cs_flops1;
        std::vector<mkp::cardset> v_cs_flops2;

        // # 1
        for (int i = 0; i < 52; ++i)
        {
            for (int j = 0; j < 52; ++j)
            {
                if (i == j)
                {
                    continue;
                }
                for (int k = 0; k < 52; ++k)
                {
                    if (i == k || j == k)
                    {
                        continue;
                    }
                    const auto c1 = mkp::card(i);
                    const auto c2 = mkp::card(j);
                    const auto c3 = mkp::card(k);

                    const auto cs = mkp::cardset{c1, c2, c3};

                    s_cs_flops1.insert(cs);
                    v_cs_flops1.push_back(cs);
                }
            }
        }

        // # 2
        for (int i = 0; i < 52; ++i)
        {
            for (int j = i + 1; j < 52; ++j)
            {
                if (i == j)
                {
                    fmt::print("this should never happen!\n");
                    continue;
                }
                for (int k = j + 1; k < 52; ++k)
                {
                    if (i == k || j == k)
                    {
                        fmt::print("this should never happen!\n");
                        continue;
                    }
                    const auto c1 = mkp::card(i);
                    const auto c2 = mkp::card(j);
                    const auto c3 = mkp::card(k);

                    const auto cs = mkp::cardset{c1, c2, c3};

                    s_cs_flops2.insert(cs);
                    v_cs_flops2.push_back(cs);
                }
            }
        }

        // compare stats
        fmt::print("size of set of flops: {} vs {}\n", s_cs_flops1.size(), s_cs_flops2.size());
        fmt::print("size of vector of flops: {} vs {}\n", v_cs_flops1.size(), v_cs_flops2.size());
    }

#if !defined(NDEBUG) && defined(NDEBUG)

    {
        std::set<mkp::cardset> s_cs_rivers1;
        std::set<mkp::cardset> s_cs_rivers2;
        std::vector<mkp::cardset> v_cs_rivers1;
        std::vector<mkp::cardset> v_cs_rivers2;

        v_cs_rivers1.reserve(311875200);
        v_cs_rivers2.reserve(2598960);

        // # 1
        for (int i = 0; i < 52; ++i)
        {
            for (int j = 0; j < 52; ++j)
            {
                if (i == j)
                {
                    continue;
                }
                for (int k = 0; k < 52; ++k)
                {
                    if (i == k || j == k)
                    {
                        continue;
                    }
                    for (int l = 0; l < 52; ++l)
                    {
                        if (i == l || j == l || k == l)
                        {
                            continue;
                        }
                        for (int m = 0; m < 52; ++m)
                        {
                            if (i == m || j == m || k == m || l == m)
                            {
                                continue;
                            }
                            //const auto c1 = mkp::card(i);
                            //const auto c2 = mkp::card(j);
                            //const auto c3 = mkp::card(k);
                            //const auto c4 = mkp::card(l);
                            //const auto c5 = mkp::card(m);
                            //const auto cs = mkp::cardset{c1, c2, c3, c4, c5};

                            const mkp::cardset cs{mkp::make_bitset(i, j, k, l, m)};

                            s_cs_rivers1.insert(cs);
                            v_cs_rivers1.push_back(cs);
                            //s_cs_rivers1.emplace(mkp::make_bitset(i, j, k, l, m));
                            //v_cs_rivers1.emplace_back(mkp::make_bitset(i, j, k, l, m));
                        }
                    }
                }
            }
        }

        // # 2
        for (int i = 0; i < 52; ++i)
        {
            for (int j = i + 1; j < 52; ++j)
            {
                if (i == j)
                {
                    fmt::print("this should never happen!\n");
                    continue;
                }
                for (int k = j + 1; k < 52; ++k)
                {
                    if (i == k || j == k)
                    {
                        fmt::print("this should never happen!\n");
                        continue;
                    }
                    for (int l = k + 1; l < 52; ++l)
                    {
                        if (i == l || j == l || k == l)
                        {
                            fmt::print("this should never happen!\n");
                            continue;
                        }
                        for (int m = l + 1; m < 52; ++m)
                        {
                            if (i == m || j == m || k == m || l == m)
                            {
                                fmt::print("this should never happen!\n");
                                continue;
                            }
                            const auto c1 = mkp::card(i);
                            const auto c2 = mkp::card(j);
                            const auto c3 = mkp::card(k);
                            const auto c4 = mkp::card(l);
                            const auto c5 = mkp::card(m);

                            const auto cs = mkp::cardset{c1, c2, c3, c4, c5};

                            s_cs_rivers2.insert(cs);
                            v_cs_rivers2.push_back(cs);
                        }
                    }
                }
            }
        }
        // compare stats
        fmt::print("size of set of rivers: {} vs {}\n", s_cs_rivers1.size(), s_cs_rivers2.size());
        fmt::print("size of vector of rivers: {} vs {}\n", v_cs_rivers1.size(), v_cs_rivers2.size());
    }
#endif    // DEBUG

    {
        // iterate over all flops
        // calculate the strength of all hands that are legal
        // infos that we wanto to store:
        // - 1326 hands with name, id
        // - for each hand, the 19600 (50 choose 3) results, that ist
        //   - score (absolute)
        //   - ranking (shared rankings will give better number, e.g., 1,2,2,2,5,6 for 3 times shared 2nd place)
        //   1326 and 19600 fit into 16 bits (uint16_t)
        // - merge results according to suit isomorphism (AcAd === AcAs etc.)
        // - total of ~52m data points
        //
        // - in each inner iteration, we 1176 (compute 49 choose 2) results

        std::vector<hand_with_info_t> all_hands{};
        all_hands.reserve(1326);
        uint16_t counter = 0;
        // prep loop: store all hands with an id
        for (uint8_t v = 0; v < 52; ++v)
        {
            for (uint8_t w = v + 1; w < 52; ++w)
            {
                const mkp::hand_2c hand{v, w};
                all_hands.push_back(hand_with_info_t{hand, counter, {}});
                //fmt::print("hand: {}, id: {}, stats stored: {}\n\n", all_hands.at(counter).m_hand.str(), all_hands.at(counter).m_id,
                //           fmt::join(all_hands.at(counter).m_stats, "\n"));
                ++counter;
            }
        }

        // outer loop: flops
        for (int i = 0; i < 1; ++i)
        {
            for (int j = i + 1; j < 2; ++j)
            {
                for (int k = j + 1; k < 52; ++k)
                {
                    // inner loop: hands

                    const mkp::cardset flop{mkp::make_bitset(i, j, k)};
                    std::vector<stats_with_id_t> results;
                    results.reserve(1176);

                    for (const auto& hand : all_hands)
                    {
                        if (const auto hand_as_cs = hand.m_hand.as_cardset(); flop.disjoint(hand_as_cs))
                        {
                            //fmt::print("evaluating {} -> ", flop.combine(hand_as_cs).str());
                            //fmt::print("result: {}\n", mkp::evaluate_safe(flop.combine(hand_as_cs)).str());
                            results.push_back(stats_with_id_t{hand.m_id, 0, mkp::evaluate_safe(flop.combine(hand_as_cs))});
                        }
                    }

                    // fmt::print("results before sorting:\n{}\n\n", fmt::join(results, "\n"));
                    // sort the results reverse (highest hand first)
                    std::sort(results.begin(), results.end(), [](const auto lhs, const auto rhs) { return lhs.m_score > rhs.m_score; });

                    // fmt::print("results after sorting / before ranking algo:\n{}\n\n", fmt::join(results, "\n"));
                    // algo to calculate ranking
                    int counter_rank_current = 1;
                    int counter = 1;
                    mkp::holdem_result last_score = results.front().m_score;
                    for (auto& res : results)
                    {
                        if (res.m_score != last_score)
                        {
                            counter_rank_current = counter;
                            last_score = res.m_score;
                        }
                        res.m_ranking = counter_rank_current;
                        ++counter;
                    }
                    // fmt::print("results after ranking algo:\n{}\n\n", fmt::join(results, "\n"));

                    // update our collection
                    for (const auto& result : results)
                    {
                        const auto it =
                            std::find_if(all_hands.begin(), all_hands.end(), [&](const auto hand) { return hand.m_id == result.m_id; });
                        if (it != all_hands.end())
                        {
                            it->m_stats.push_back({result.m_score, result.m_ranking});
                        }
                    }
                }
            }
        }

        // print some results
        for (const auto& hand : all_hands)
        {
            //fmt::print("hand: {}\n", hand.m_hand.str());
            //fmt::print("hand: {}, id: {}\n", hand.m_hand.str(), hand.m_id);
            //if (hand.m_stats.size() > 0)
            //{
            //    auto test = hand.m_stats;
            //    fmt::print("stats: {}\n", fmt::join(hand.m_stats, "\n"));
            //}
            fmt::print("hand: {}, id: {}, stats stored:\n{}\n\n", hand.m_hand.str(), hand.m_id, fmt::join(hand.m_stats, "\n"));
        }

        // todo:

        // print sizes to check

        // average ranks
        // print results

        // merge according to suit isomorphism
        // print results
    }
    // todo:
    // - compute rankings the traditional way
    // - compute rankings with generators for hands and river
}