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
#include <mkpoker/base/range.hpp>
#include <mkpoker/holdem/holdem_evaluation.hpp>

#include <algorithm>    // sort, find_if, transform
#include <atomic>       //
#include <chrono>       // ms, high_resolution_clock
#include <execution>    // par_unseq
#include <mutex>        //
#include <numeric>      // reduce
#include <set>          //
#include <string>       //
#include <thread>       // thread, sleep_for
#include <vector>       //

#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <sqlite/sqlite3.h>

struct hand_with_stats_t
{
    uint64_t m_sum;
    std::vector<uint32_t> m_rankings;
    uint16_t m_id;
};

struct stats_with_id_t
{
    mkp::holdem_result m_score;
    uint32_t m_ranking;
    uint16_t m_id;
};

struct ranking_t
{
    uint32_t m_ranking;
    uint16_t m_id;
};

// fmt 8.0.1 bug
// https://godbolt.org/z/KWvsq9dez

template <>
struct fmt::formatter<ranking_t>
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
    auto format(const ranking_t& rt, FormatContext& ctx) -> decltype(ctx.out())
    {
        // ctx.out() is an output iterator to write to.
        return format_to(ctx.out(), "(id:{:>4}, ranking:{:>9})", rt.m_id, rt.m_ranking);
    }
};

template <>
struct fmt::formatter<hand_with_stats_t>
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
    auto format(const hand_with_stats_t& hws, FormatContext& ctx) -> decltype(ctx.out())
    {
        // ctx.out() is an output iterator to write to.
        return format_to(ctx.out(), "(id:{:>4}, sum:{:>9})", hws.m_id, hws.m_sum);
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
    /*
    {
        // iterate over all turns
        // calculate the strength of all hands that are legal
        // infos that we wanto to store:
        // - 1326 (52 chosse 2) hands with name, id
        // - for each hand, the 230300 (50 choose 4) rankings
        // total of ~305m data points
        //   1326 -> 16 bit
        //   230300 -> 32 bit
        // - merge results according to suit isomorphism (AcAd === AcAs etc.)
        //
        // 270725 (52 choose 4) trials
        // in each trial, we get 1128 (compute 48 choose 2) results
        // total of ~305m data points

        std::vector<mkp::hand_2c> hands_index{};
        std::vector<hand_with_stats_t> all_hands{};
        hands_index.reserve(1326);
        all_hands.reserve(1326);
        std::vector<ranking_t> all_results;
        all_results.reserve(305377800);
        std::mutex mu;

        {
            // prep loop: store all hands with an id
            uint16_t counter = 0;
            for (uint8_t v = 0; v < 52; ++v)
            {
                for (uint8_t w = v + 1; w < 52; ++w)
                {
                    const mkp::hand_2c hand{v, w};
                    hands_index.push_back(hand);
                    all_hands.push_back(hand_with_stats_t{0, {}, counter});
                    ++counter;
                }
            }
        }

        const auto t1 = std::chrono::high_resolution_clock::now();

        const auto c_threads_max = std::thread::hardware_concurrency();
        std::vector<std::atomic<bool>> threads_running(c_threads_max);

        // outer loop: turns
        fmt::print("starting evaluation of turns...\n");
        for (int i = 0; i < 52; ++i)
        {
            for (int j = i + 1; j < 52; ++j)
            {
                {
                    // inner loop: hands
                    auto fn = [=, &threads_running, &all_results, &mu](int z) {
                        //fmt::print("thread {} started\n", z);
                        std::vector<ranking_t> final_results;
                        for (int k = j + 1; k < 52; ++k)
                        {
                            for (int l = k + 1; l < 52; ++l)
                            {
                                {
                                    const mkp::cardset turn{mkp::make_bitset(i, j, k, l)};
                                    std::vector<stats_with_id_t> results;
                                    results.reserve(1128);

                                    for (const auto& hand : all_hands)
                                    {
                                        if (const auto hand_as_cs = hands_index.at(hand.m_id).as_cardset(); turn.disjoint(hand_as_cs))
                                        {
                                            results.push_back(stats_with_id_t{mkp::evaluate_safe(turn.combine(hand_as_cs)), 0, hand.m_id});
                                        }
                                    }

                                    // fmt::print("results before sorting:\n{}\n\n", fmt::join(results, "\n"));
                                    // sort the results reverse (highest hand first)
                                    std::sort(results.begin(), results.end(),
                                              [](const auto& lhs, const auto& rhs) { return lhs.m_score > rhs.m_score; });

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
                                    std::transform(results.begin(), results.end(), std::back_inserter(final_results),
                                                   [](const auto& st) -> ranking_t {
                                                       return {st.m_ranking, st.m_id};
                                                   });
                                }
                                // fmt::print("results after ranking algo:\n{}\n\n", fmt::join(results, "\n"));
                            }
                        }

                        // update the collection
                        {
                            std::lock_guard<std::mutex> guard(mu);
                            all_results.insert(all_results.end(), std::make_move_iterator(final_results.begin()),
                                               std::make_move_iterator(final_results.end()));
                        }
                        //fmt::print("thread {} ended\n", z);
                        threads_running[z] = false;
                    };

                    for (;;)
                    {
                        bool thread_spawned = false;
                        for (int z = 0; z < threads_running.size(); ++z)
                        {
                            if (!threads_running[z])
                            {
                                threads_running[z] = true;
                                std::thread t(fn, z);
                                t.detach();
                                thread_spawned = true;
                                break;
                            }
                        }
                        if (thread_spawned)
                        {
                            break;
                        }
                        else
                        {
                            std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        }
                    }
                }
            }
        }

        auto finish_threads = [&]() {
            fmt::print("waiting for threads to finish...\n\n");
            bool still_running = false;
            for (;;)
            {
                for (int z = 0; z < threads_running.size(); ++z)
                {
                    if (threads_running[z])
                    {
                        still_running = true;
                        break;
                    }
                }

                if (!still_running)
                {
                    break;
                }
                else
                {
                    still_running = false;
                }
            }
        };
        // wait for all threads to finish
        finish_threads();

        const auto t2 = std::chrono::high_resolution_clock::now();
        const std::chrono::duration<double, std::milli> duration = t2 - t1;
        fmt::print("duration for evaluating and appending {} hands: {} ms\n", all_results.size(), duration.count());

        // sort
        fmt::print("start sorting data...\n");
        const auto sort_t1 = std::chrono::high_resolution_clock::now();
        std::sort(std::execution::par_unseq, all_results.begin(), all_results.end(),
                  [](const auto& lhs, const auto& rhs) { return lhs.m_id < rhs.m_id; });
        const auto sort_t2 = std::chrono::high_resolution_clock::now();
        const std::chrono::duration<double, std::milli> duration_sort = sort_t2 - sort_t1;
        fmt::print("duration for sorting {} hands: {} ms\n", all_results.size(), duration_sort.count());

        fmt::print("\nstarting evaluation of data...\n");
        const auto combine_t1 = std::chrono::high_resolution_clock::now();
        int16_t counter = 0;
        for (auto& hand : all_hands)
        {
            const auto it_start = all_results.cbegin() + (static_cast<int64_t>(counter) * 230300);
            const auto it_end = all_results.cbegin() + (static_cast<int64_t>(++counter) * 230300);
            hand.m_sum = std::reduce(it_start, it_end, uint64_t(0),
                                     [](const auto& val, const auto& elem) -> uint64_t { return val + elem.m_ranking; });
        }
        const auto combine_t2 = std::chrono::high_resolution_clock::now();
        const std::chrono::duration<double, std::milli> duration_combine = combine_t2 - combine_t1;
        fmt::print("duration for evaluating {} hands: {} ms\n", all_results.size(), duration_combine.count());
        all_results.clear();

        // sort hands
        std::sort(all_hands.begin(), all_hands.end(), [](const auto& lhs, const auto& rhs) { return lhs.m_sum < rhs.m_sum; });

        fmt::print("\nRanking after turn:\n");
        {
            int counter = 1;
            for (auto& hand : all_hands)
            {
                fmt::print("#{:>4}: {}/{:>4} with score {:>8} (average: {:>4})\n", counter, hands_index.at(hand.m_id).str(), hand.m_id,
                           hand.m_sum, hand.m_sum / 230300);
                ++counter;
            }
        }

        std::set<uint8_t> stored_starting_hands;
        for (auto it = all_hands.begin(); it != all_hands.end();)
        {
            const auto index = mkp::range::index(hands_index.at(it->m_id));
            if (stored_starting_hands.contains(index))
            {
                it = all_hands.erase(it);
            }
            else
            {
                stored_starting_hands.insert(index);
                ++it;
            }
        }

        fmt::print("\nRanking after suit isomorphism:\n");
        {
            int counter = 1;
            for (auto& hand : all_hands)
            {
                fmt::print("#{:>4}: {}/{:>4} with score {:>8} (average: {:>4})\n", counter, hands_index.at(hand.m_id).str(), hand.m_id,
                           hand.m_sum, hand.m_sum / 230300);
                ++counter;
            }
        }
    }
    */

    // with full extent

    {
        // iterate over all rivers
        // calculate the strength of all hands that are legal
        // infos that we wanto to store:
        // - 1326 (52 chosse 2) hands with name, id
        // - for each hand, the 2118760 (50 choose 5) rankings
        // total of ~2.8b data points
        //   1326 -> 16 bit
        //   2118760 -> 32 bit
        // - merge results according to suit isomorphism (AcAd === AcAs etc.)
        //
        // 2598960 (52 choose 5) trials
        // in each trial, we get 1081 (compute 47 choose 2) results
        // total of ~2.8b data points

        std::vector<mkp::hand_2c> hands_index{};
        std::vector<hand_with_stats_t> all_hands{};
        hands_index.reserve(1326);
        all_hands.reserve(1326);
        std::vector<ranking_t> all_results;
        std::vector<ranking_t> all_results_swap;
        std::mutex mu;

        {
            // prep loop: store all hands with an id
            uint16_t counter = 0;
            for (uint8_t v = 0; v < 52; ++v)
            {
                for (uint8_t w = v + 1; w < 52; ++w)
                {
                    const mkp::hand_2c hand{v, w};
                    hands_index.push_back(hand);
                    all_hands.push_back(hand_with_stats_t{0, {}, counter});
                    ++counter;
                }
            }
        }

        all_results.reserve(134217728);
        const auto t1 = std::chrono::high_resolution_clock::now();

        const auto c_threads_max = std::thread::hardware_concurrency();
        std::vector<std::atomic<bool>> threads_running(c_threads_max);

        std::atomic<int64_t> total_cleanup = 0;
        std::atomic<int64_t> total_threads = 0;

        // outer loop: rivers
        fmt::print("starting evaluation of rivers...\n");
        for (int i = 0; i < 52; ++i)
        {
            for (int j = i + 1; j < 52; ++j)
            {
                {
                    // inner loop: hands
                    auto fn = [=, &threads_running, &all_results, &mu, &total_threads](int z) {
                        //fmt::print("thread {} started\n", z);
                        std::vector<ranking_t> final_results;
                        for (int k = j + 1; k < 52; ++k)
                        {
                            for (int l = k + 1; l < 52; ++l)
                            {
                                for (int m = l + 1; m < 52; ++m)
                                {
                                    const mkp::cardset river{mkp::make_bitset(i, j, k, l, m)};
                                    std::vector<stats_with_id_t> results;
                                    results.reserve(1081);

                                    for (const auto& hand : all_hands)
                                    {
                                        if (const auto hand_as_cs = hands_index.at(hand.m_id).as_cardset(); river.disjoint(hand_as_cs))
                                        {
                                            results.push_back(stats_with_id_t{mkp::evaluate_safe(river.combine(hand_as_cs)), 0, hand.m_id});
                                        }
                                    }

                                    // fmt::print("results before sorting:\n{}\n\n", fmt::join(results, "\n"));
                                    // sort the results reverse (highest hand first)
                                    std::sort(results.begin(), results.end(),
                                              [](const auto& lhs, const auto& rhs) { return lhs.m_score > rhs.m_score; });

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
                                    std::transform(results.begin(), results.end(), std::back_inserter(final_results),
                                                   [](const auto& st) -> ranking_t {
                                                       return {st.m_ranking, st.m_id};
                                                   });
                                }
                                // fmt::print("results after ranking algo:\n{}\n\n", fmt::join(results, "\n"));
                            }
                        }

                        if (final_results.size() > 0)
                        {
                            total_threads += final_results.size();
                        }
                        // update the collection
                        if (final_results.size() > 0)
                        {
                            std::lock_guard<std::mutex> guard(mu);
                            all_results.insert(all_results.end(), std::make_move_iterator(final_results.begin()),
                                               std::make_move_iterator(final_results.end()));
                        }
                        //fmt::print("thread {} ended\n", z);
                        threads_running[z] = false;
                    };

                    for (;;)
                    {
                        bool thread_spawned = false;
                        for (int z = 0; z < threads_running.size(); ++z)
                        {
                            if (!threads_running[z])
                            {
                                threads_running[z] = true;
                                std::thread t(fn, z);
                                t.detach();
                                thread_spawned = true;
                                break;
                            }
                        }
                        if (thread_spawned)
                        {
                            break;
                        }
                        else
                        {
                            if (all_results.size() > 100'000'000)
                            {
                                fmt::print("\nstarting data cleanup for {} hands...\n", all_results.size());
                                const auto cleanup_t1 = std::chrono::high_resolution_clock::now();
                                // swap vectors
                                {
                                    std::lock_guard<std::mutex> guard(mu);
                                    std::swap(all_results, all_results_swap);
                                    //all_results.clear();
                                    //all_results.shrink_to_fit();
                                    all_results.reserve(134217728);
                                }
                                total_cleanup += all_results_swap.size();
                                // sort
                                std::sort(std::execution::par_unseq, all_results_swap.begin(), all_results_swap.end(),
                                          [](const auto& lhs, const auto& rhs) { return lhs.m_id < rhs.m_id; });
                                // sum
                                auto it_end = all_results_swap.begin();
                                auto it_start = it_end;
                                for (;;)
                                {
                                    it_start = it_end;
                                    it_end = std::find_if(it_start, all_results_swap.end(),
                                                          [&](const auto& rt) { return rt.m_id > it_start->m_id; });

                                    all_hands.at(it_start->m_id).m_sum +=
                                        std::reduce(it_start, it_end, uint64_t(0),
                                                    [](const auto& val, const auto& elem) -> uint64_t { return val + elem.m_ranking; });

                                    if (it_end == all_results_swap.end())
                                    {
                                        break;
                                    }
                                }
                                all_results_swap.clear();
                                const auto cleanup_t2 = std::chrono::high_resolution_clock::now();
                                const std::chrono::duration<double, std::milli> duration_cleanup = cleanup_t2 - cleanup_t1;
                                fmt::print("... end data cleanup - duration: {} ms\n", duration_cleanup.count());
                            }
                            else
                            {
                                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                            }
                        }
                    }
                }
            }
        }

        auto finish_threads = [&]() {
            fmt::print("\nwaiting for last threads to finish...\n");
            bool still_running = false;
            for (;;)
            {
                for (int z = 0; z < threads_running.size(); ++z)
                {
                    if (threads_running[z])
                    {
                        still_running = true;
                        break;
                    }
                }

                if (!still_running)
                {
                    break;
                }
                else
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    still_running = false;
                }
            }
        };
        // wait for all threads to finish
        finish_threads();
        fmt::print("cleanup for remaining {} hands...\n\n", all_results.size());

        // sort
        std::sort(std::execution::par_unseq, all_results.begin(), all_results.end(),
                  [](const auto& lhs, const auto& rhs) { return lhs.m_id < rhs.m_id; });
        // sum
        auto it_end = all_results.begin();
        auto it_start = it_end;
        for (;;)
        {
            it_start = it_end;
            it_end = std::find_if(it_start, all_results.end(), [&](const auto& rt) { return rt.m_id > it_start->m_id; });

            all_hands.at(it_start->m_id).m_sum += std::reduce(
                it_start, it_end, uint64_t(0), [](const auto& val, const auto& elem) -> uint64_t { return val + elem.m_ranking; });

            if (it_end == all_results.end())
            {
                break;
            }
        }
        total_cleanup += all_results.size();
        all_results.clear();
        all_results.shrink_to_fit();

        // sort hands
        std::sort(all_hands.begin(), all_hands.end(), [](const auto& lhs, const auto& rhs) { return lhs.m_sum < rhs.m_sum; });
        const auto t2 = std::chrono::high_resolution_clock::now();
        const std::chrono::duration<double, std::milli> duration = t2 - t1;
        fmt::print("total number of hands touched by cleanup: {}\n", total_cleanup);
        fmt::print("total number of hands touched by threads: {}\n", total_threads);
        fmt::print("duration for evaluating and sorting {} hands: {} ms\n", int64_t(2598960) * 1081, duration.count());

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
        rc = sqlite3_open("river_results.db", &db);
        if (rc)
        {
            fmt::print("Can't open database: {}\n", sqlite3_errmsg(db));
            return EXIT_FAILURE;
        }
        else
        {
            fmt::print("Opened database successfully\n");
        }
        // create table
        const auto sql_create_table =
            "CREATE TABLE RIVERS_V1("
            "RANK INT,"
            "HAND TEXT,"
            "ID INT,"
            "SUM INT,"
            "AVERAGE INT"
            ");";
        rc = sqlite3_exec(
            db, sql_create_table, [](void*, int, char**, char**) -> int { return 0; }, 0, &zErrMsg);
        check_answer_and_print("Table created successfully\n");
        // fill table
        std::string sql_insert_data = "INSERT INTO RIVERS_V1 (RANK, HAND, ID, SUM, AVERAGE) VALUES\n";
        {
            int counter = 1;
            for (auto& hand : all_hands)
            {
                sql_insert_data += fmt::format("({:>4}, '{}', {:>4}, {:>10}, {:>3}),\n", counter, hands_index.at(hand.m_id).str(),
                                               hand.m_id, hand.m_sum, hand.m_sum / 2598960);
                ++counter;
            }
        }
        sql_insert_data.pop_back();
        sql_insert_data.back() = ';';
        fmt::print("\n{}\n", sql_insert_data);
        rc = sqlite3_exec(
            db, sql_insert_data.c_str(), [](void*, int, char**, char**) -> int { return 0; }, 0, &zErrMsg);
        check_answer_and_print("Data inserted successfully\n");
        // end sqlite

        std::set<uint8_t> stored_starting_hands;
        for (auto it = all_hands.begin(); it != all_hands.end();)
        {
            const auto index = mkp::range::index(hands_index.at(it->m_id));
            if (stored_starting_hands.contains(index))
            {
                it = all_hands.erase(it);
            }
            else
            {
                stored_starting_hands.insert(index);
                ++it;
            }
        }

        // create table
        const auto sql_create_table2 =
            "CREATE TABLE RIVERS_AFTER_V1("
            "RANK INT,"
            "HAND TEXT,"
            "ID INT,"
            "SUM INT,"
            "AVERAGE INT"
            ");";
        rc = sqlite3_exec(
            db, sql_create_table2, [](void*, int, char**, char**) -> int { return 0; }, 0, &zErrMsg);
        check_answer_and_print("Table created successfully\n");
        // fill table
        std::string sql_insert_data2 = "INSERT INTO RIVERS_AFTER_V1 (RANK, HAND, ID, SUM, AVERAGE) VALUES\n";
        {
            int counter = 1;
            for (auto& hand : all_hands)
            {
                const auto ind = mkp::range::index(hands_index.at(hand.m_id));
                sql_insert_data2 += fmt::format("({:>3}, '{}', {:>4}, {:>10}, {:>3}),\n", counter, mkp::range::hand(ind).str(), hand.m_id,
                                                hand.m_sum, hand.m_sum / 2598960);
                ++counter;
            }
        }
        sql_insert_data2.pop_back();
        sql_insert_data2.back() = ';';
        fmt::print("\n{}\n", sql_insert_data2);
        rc = sqlite3_exec(
            db, sql_insert_data2.c_str(), [](void*, int, char**, char**) -> int { return 0; }, 0, &zErrMsg);
        check_answer_and_print("Data inserted successfully\n");
        // end sqlite

        sqlite3_close(db);
    }
}