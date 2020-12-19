/*

mkpoker - demo command line app that prints all different hand strengths

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

#include <mkpoker/base/cardset.hpp>
#include <mkpoker/holdem/holdem_evaluation.hpp>

#include <algorithm>
#include <iostream>
#include <vector>

int main()
{
    std::vector<mkp::holdem_result> results;
    results.reserve(2598960);    // 52 choose 5

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
                        const mkp::cardset cards{mkp::make_bitset(i, j, k, l, m)};
                        // store results
                        results.emplace_back(mkp::evaluate_safe(cards));
                    }
                }
            }
        }
    }

    // after removing duplicates, there should be 7642 distinct hand strenghts left

    std::cout << "size before unique: " << results.size() << "\n";
    std::sort(results.begin(), results.end());
    const auto last = std::unique(results.begin(), results.end());
    results.erase(last, results.end());
    std::cout << "size after unique: " << results.size() << "\n";

    return EXIT_SUCCESS;
}