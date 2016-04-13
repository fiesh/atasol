/*
 * Copyright (c) 2016, Christoph Weiss
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Uncomment to play white
//#define WHITEHUMAN

// Uncomment to plya black
//#define BLACKHUMAN

// Depth of white computer player
#define WHITEDEPTH 5

// Depth of black computer player
#define BLACKDEPTH 3

#include <iostream>
#include <regex>
#include <string>
#include <utility>

#include "solver.hpp"

namespace {
#if defined(WHITEHUMAN) || defined(BLACKHUMAN)
	inline std::pair<bool, atasol::Status> getHumanInput(atasol::Status const & status)
	{
		using namespace atasol;
		std::array<Status, upperLimitMoves> possibleMoves;
		const auto numMoves = generateStatuses(status, possibleMoves.begin());
		std::string i;
		Status newStatus;
		for(;;) {
			newStatus = status;
			i.clear();
			std::cout << "> ";
			std::getline(std::cin, i);
			if(std::cin.eof()) {
				return std::make_pair(false, newStatus);
			}
			static const std::regex spawnRegex(R"widdae(^[[:space:]]*([[:alpha:]])[[:space:]]*([[:digit:]][[:digit:]]*)[[:space:]]*$)widdae", std::regex::optimize | std::regex::extended);
			static const std::regex jumpRegex(R"widdae(^[[:space:]]*([[:alpha:]])[[:space:]]*([[:digit:]][[:digit:]]*)[[:space:]]*([[:alpha:]])[[:space:]]*([[:digit:]][[:digit:]]*)[[:space:]]*$)widdae", std::regex::optimize | std::regex::extended);

			std::smatch m;

			// Convert something like "B6" to the coordinate value 2 * boardSize + 6, and return -1 if this was not possible
			static const auto parseCoordinate = [] (const auto & s0, const auto & s1) {
				assert(s0.size() == 1);
				assert(s1.size() >= 1);
				int col;
				if(s0[0] >= 'a' && s0[0] <= 'z') {
					col = s0[0] - 'a';
				} else {
					col = s0[0] - 'A';
				}
				if(!(col >= 0 && col < static_cast<int>(boardSize))) {
					// Didn't work, prompt again.
					return static_cast<uint32_t>(-1);
				}
				int row;
				try {
					row = std::stoi(s1);
				} catch(...) {
					return static_cast<uint32_t>(-1);
				}
				if(!(row >= 0 && row < static_cast<int>(boardSize))) {
					return static_cast<uint32_t>(-1);
				}
				return static_cast<uint32_t>(row) * boardSize + static_cast<uint32_t>(col);
			};
			if(std::regex_match(i, m, spawnRegex) && m.size() == 3) {
				const auto newBlob = parseCoordinate(m[1].str(), m[2].str());
				if(newBlob == static_cast<uint32_t>(-1)) {
					continue;
				} else {
					// Spawn a new blob.
					assert(newBlob < boardSize * boardSize);
					newStatus.spawn(newBlob / boardSize, newBlob % boardSize);
					if(std::find(possibleMoves.cbegin(), possibleMoves.cbegin() + numMoves, newStatus) == possibleMoves.cbegin() + numMoves) {
						std::cout << "Illegal move!\n";
						continue;
					} else {
						break;
					}
				}
			} else if(std::regex_match(i, m, jumpRegex) && m.size() == 5) {
				const auto newBlob = parseCoordinate(m[3].str(), m[4].str());
				const auto oldBlob = parseCoordinate(m[1].str(), m[2].str());
				if(newBlob == static_cast<uint32_t>(-1) ||
						oldBlob == static_cast<uint32_t>(-1)) {
					continue;
				} else {
					assert(oldBlob < boardSize * boardSize);
					assert(newBlob < boardSize * boardSize);
					// Remove blob that jumped.
					newStatus.set(oldBlob);
					newStatus.spawn(newBlob / boardSize, newBlob % boardSize);
					if(std::find(possibleMoves.cbegin(), possibleMoves.cbegin() + numMoves, newStatus) == possibleMoves.cbegin() + numMoves) {
						std::cout << "Illegal move!\n";
						continue;
					} else {
						break;
					}
				}
			}
		}

		return std::make_pair(true, newStatus);
	}
#endif
}

int main()
{
	using namespace atasol;
	Status status;

	status.set(0 * boardSize + 0, Entry::White);
	status.set(0 * boardSize + boardSize - 1, Entry::Black);
	status.set((boardSize - 1) * boardSize + 0, Entry::Black);
	status.set((boardSize - 1) * boardSize + boardSize - 1, Entry::White);

	int moveNum = 0;
		std::cout << status.to_string();
	for(;;) {
		if(std::abs(status.score()) >= static_cast<Score>(boardSize * boardSize)) {
			break;
		}
		std::cout << "======== Move " << moveNum++ << " ========\n";

#ifdef WHITEHUMAN
		{
			auto s = getHumanInput(status);
			if(!s.first) {
				break;
			}
			status = std::move(s.second);
		}
#else
		{
			Status newStatus;
			minimax<WHITEDEPTH>(status, &newStatus);
			std::cout << "> " << moveString(status, newStatus) << '\n';
			status = std::move(newStatus);
		}
#endif
		std::cout << status.to_string();
		if(std::abs(status.score()) >= static_cast<Score>(boardSize * boardSize)) {
			break;
		}

#ifdef BLACKHUMAN
		{
			auto s = getHumanInput(status);
			if(!s.first) {
				break;
			}
			status = std::move(s.second);
		}
#else
		{
			Status newStatus;
			minimax<BLACKDEPTH>(status, &newStatus);
			std::cout << "> " << moveString(status, newStatus) << '\n';
			status = std::move(newStatus);
		}
#endif
		std::cout << status.to_string();
	}

	return 0;
}
