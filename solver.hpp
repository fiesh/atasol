/*!
 * \file atasol_solver.hpp
 * \brief atasol solver routines
 *
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

#ifndef ATASOL_SOLVER_HPP_
#define ATASOL_SOLVER_HPP_

#include <cassert>
#include <cstdint>

#include <algorithm>
#include <array>
#include <sstream>
#include <string>
#include <utility>

namespace atasol {
	//! The size of the game board.  Defaults to 7 since normally a 7x7 board is used.
	static constexpr uint32_t boardSize = 7;

	static_assert(boardSize > 2, "Board Size must be at least 3.");

	using Score = int32_t;

	//! The underlying storage size.
	using Storage = std::size_t;

	//! The three possible states of each field
	enum class Entry
	{
		Empty = 0,
		White = 1,
		Black = 2,
	};

	namespace detail {
		// The game itself is played on an boardSize * boardSize board, with each Entry having three possible values.
		// We just use 2 bits per such state, allowing for an unused fourth value.
		// Furthermore, we want one bit to represent if it's the first or the second player's turn.
		constexpr uint32_t bitsNeeded = boardSize * boardSize * 2 + 1;
		constexpr uint32_t bitsPerStorage = sizeof(Storage) * 8;
		constexpr uint32_t storagesNeeded = bitsNeeded / bitsPerStorage + (bitsNeeded % bitsPerStorage != 0);
	}

	// An upper bound for the number of possible moves of one player.
	// Used so that we don't have to do dynamic memory allocation.
	// TODO: This could be tweaked further by doing some thinking.
	constexpr uint32_t upperLimitMoves =
		boardSize * boardSize // One blob may be created in at most every empty space
		+ boardSize * boardSize * 16 // Every blob may jump to at most 16 other spaces
		;

	//! The status of a game
	class Status
	{
		public:
			constexpr Status() :
				storages_{} {}

			constexpr bool whiteMoves() const noexcept { return (storages_[storages_.size() - 1] & (1UL << (detail::bitsPerStorage - 1))) == 0; }
			constexpr bool blackMoves() const noexcept { return (storages_[storages_.size() - 1] & (1UL << (detail::bitsPerStorage - 1))) != 0; }

			constexpr Entry movingPlayer() const noexcept { return whiteMoves() ? Entry::White : Entry::Black; }

			//! Switch to the other player
			void switchPlayerTurn() noexcept { storages_[storages_.size() - 1] ^= (1UL << (detail::bitsPerStorage - 1)); }

			//! Access individual entry
			constexpr Entry operator[](uint32_t pos) const noexcept
			{
				assert(pos < boardSize * boardSize);
				pos *= 2;
				const auto index = pos / detail::bitsPerStorage;
				const auto relPos = pos - index * detail::bitsPerStorage;
				const Storage bitmask = 3UL << relPos;
				assert(index < storages_.size());
				return static_cast<Entry>((storages_[index] & bitmask) >> relPos);
			}

			//! Set an individual entry
			void set(const uint32_t pos, const Entry value = Entry::Empty) noexcept
			{
				const auto v = operator[](pos);
				if(v == value) {
					return;
				}
				if(v == Entry::White) {
					--whiteScore_;
				} else if(v == Entry::Black) {
					--blackScore_;
				}
				if(value == Entry::White) {
					++whiteScore_;
				} else if(value == Entry::Black) {
					++blackScore_;
				}
				setNoScoreAdjust(pos, value);
			}

			//! Put a blob at an entry, making the surrounding entries of opponent color change color
			/*!
			 * This function spawns a blob at coordinates (i, j) of the current player.
			 * It alters all surrounding blobs under opponent control to the current player's color.
			 * It then changes the current player.
			 */
			void spawn(uint32_t i, uint32_t j) noexcept
			{
				assert(i < boardSize);
				assert(j < boardSize);
				// Current player
				const auto cp = movingPlayer();
				setNoScoreAdjust(i * boardSize + j, cp);
				if(whiteMoves()) {
					++whiteScore_;
				} else {
					++blackScore_;
				}
				switchPlayerTurn();
				// Next player
				const auto np = movingPlayer();
				if(i > 0) {
					if(j > 0 && operator[]((i - 1) * boardSize + j - 1) == np) {
						set((i - 1) * boardSize + j - 1, cp);
					}
					if(operator[]((i - 1) * boardSize + j) == np) {
						set((i - 1) * boardSize + j, cp);
					}
					if(j < boardSize - 1 && operator[]((i - 1) * boardSize + j + 1) == np) {
						set((i - 1) * boardSize + j + 1, cp);
					}
				}
				if(j > 0 && operator[](i * boardSize + j - 1) == np) {
					set(i * boardSize + j - 1, cp);
				}
				if(j < boardSize - 1 && operator[](i * boardSize + j + 1) == np) {
					set(i * boardSize + j + 1, cp);
				}
				if(i < boardSize - 1) {
					if(j > 0 && operator[]((i + 1) * boardSize + j - 1) == np) {
						set((i + 1) * boardSize + j - 1, cp);
					}
					if(operator[]((i + 1) * boardSize + j) == np) {
						set((i + 1) * boardSize + j, cp);
					}
					if(j < boardSize - 1 && operator[]((i + 1) * boardSize + j + 1) == np) {
						set((i + 1) * boardSize + j + 1, cp);
					}
				}
			}

			//! Compute the score
			/*!
			 * The score is simply the sum over all f(e) for every entry e, where
			 * f(black) = -1, f(white) = 1, f(empty) = 0.
			 * However, when the board is full or white or black have no blobs left, the score is +- infinity.
			 */
			Score score() const noexcept
			{
#ifndef NDEBUG
				Score w = 0, b = 0;
				for(uint32_t i = 0; i != boardSize * boardSize; ++i) {
					const auto v = operator[](i);
					if(v == Entry::Black) {
						++b;
					} else if(v == Entry::White) {
						++w;
					}
				}
				assert(w == whiteScore_);
				assert(b == blackScore_);
#endif
				if(blackScore_ == 0) {
					// Black lost.
					return static_cast<Score>(boardSize * boardSize);
				} else if(whiteScore_ == 0) {
					// White lost.
					return - static_cast<Score>(boardSize * boardSize);
				} else if(whiteScore_ + blackScore_ == boardSize * boardSize) {
					// The board is full, so the winner takes it all.
					if(whiteScore_ > blackScore_) {
						return boardSize * boardSize;
					} else if(blackScore_ > whiteScore_) {
						return - static_cast<Score>(boardSize * boardSize);
					}
				}
				return whiteScore_ - blackScore_;
			}

			//! Convert this Status to string
			template<class CharT = char,
				class Traits = std::char_traits<CharT>,
				class Allocator = std::allocator<CharT>>
					std::basic_string<CharT, Traits, Allocator> to_string(CharT empty = CharT('E'), CharT white = CharT('W'), CharT black = CharT('B')) const
					{
						std::basic_string<CharT, Traits, Allocator> r;
						constexpr std::size_t size = (4 + boardSize * 2 + 1) * (boardSize + 3);
						r.reserve(size);
						{
							r += "  | ";
							char col = 'A';
							for(uint32_t i = 0; i != boardSize; ++i) {
								r += col;
								r += ' ';
								++col;
							}
							r += '\n';
						}
						r += "--+-";
						for(uint32_t i = 0; i != boardSize; ++i) {
							r += "--";
						}
						r += '\n';
						char row = '0';
						for(uint32_t i = 0; i != boardSize; ++i) {
							r += row;
							++row;
							r += " | ";
							for(uint32_t j = 0; j != boardSize; ++j) {
								const auto v = operator[](i * boardSize + j);
								r += v == Entry::Empty ? empty :
									v == Entry::White ? white :
									black;
								r += ' ';
							}
							r += '\n';
						}
						r += "--+-";
						for(uint32_t i = 0; i != boardSize; ++i) {
							r += "--";
						}
						r += '\n';
						r += "  | Score: ";
						{
							std::stringstream ss;
							ss << score();
							r += ss.str();
						}
						r += '\n';
						return r;
					}

		private:
			//! The status as a bit field
			std::array<Storage, detail::storagesNeeded> storages_;

			//! The current score of white
			Score whiteScore_ = 0;

			//! The current score of black
			Score blackScore_ = 0;

			//! Set an individual entry without adjusting score
			void setNoScoreAdjust(uint32_t pos, const Entry value) noexcept
			{
				assert(pos < boardSize * boardSize);
				pos *= 2;
				const auto index = pos / detail::bitsPerStorage;
				const auto relPos = pos - index * detail::bitsPerStorage;
				const Storage bitmask = 3UL << relPos;
				assert(index < storages_.size());
				storages_[index] &= ~bitmask;
				storages_[index] |= static_cast<Storage>(value) << relPos;
			}

			friend bool operator==(Status const & lhs, Status const & rhs) noexcept
			{
				assert(lhs.storages_ != rhs.storages_ || (lhs.whiteScore_ == rhs.whiteScore_ && lhs.blackScore_ == rhs.blackScore_));
				return lhs.storages_ == rhs.storages_;
			}
	};

	//! Generate all possible moves
	/*!
	 * This function generates all possibles new Status that may result from an existing Status.
	 * \tparam Iter The type of the output iterator
	 * \param[in] start The initial Status from which to start
	 * \param[in] output The output iterator that is written to, must be sufficiently large to hold all generated Statuses
	 * \return The number of Statuses generated
	 */
	template<typename Iter>
		uint32_t generateStatuses(Status const & start, Iter output) noexcept
		{
			uint32_t num = 0;
			const auto movingPlayer = start.movingPlayer();
			for(uint32_t i = 0; i != boardSize; ++i) {
				for(uint32_t j = 0; j != boardSize; ++j) {
					if(start[i * boardSize + j] != Entry::Empty) {
						// Entry must still be empty.
						continue;
					}
					// First, go through all possibilities to spawn a new blob from an existing one.
					// Check that at least one neighboring entry is of type movingPlayer
					if(
							(i > 0 && (
								   (start[(i - 1) * boardSize + j] == movingPlayer) ||
								   (j > 0 && start[(i - 1) * boardSize + j - 1] == movingPlayer) ||
								   (j < boardSize - 1 && start[(i - 1) * boardSize + j + 1] == movingPlayer)
								  )) ||
							((
							  (j > 0 && start[i * boardSize + j - 1] == movingPlayer) ||
							  (j < boardSize - 1 && start[i * boardSize + j + 1] == movingPlayer)
							 )) ||
							(i < boardSize - 1 && (
									       (start[(i + 1) * boardSize + j] == movingPlayer) ||
									       (j > 0 && start[(i + 1) * boardSize + j - 1] == movingPlayer) ||
									       (j < boardSize - 1 && start[(i + 1) * boardSize + j + 1] == movingPlayer)
									      ))
					  ) {
						// There is a neighboring blob that the one at (i, j) could be spawned from.
						auto copy = start;
						copy.spawn(i, j);
						*output = copy;
						++output;
						++num;
					}
					// Next, check if an existing blob can jump to the given position.
					// For every such possibility, we need to append an individual Status.
					if(i > 1) {
						// Check if a blob can jump in from two rows above.
						if(j > 0) {
							if(j > 1 && start[(i - 2) * boardSize + j - 2] == movingPlayer) {
								auto copy = start;
								copy.set((i - 2) * boardSize + j - 2);
								copy.spawn(i, j);
								*output = copy;
								++output;
								++num;
							}
							if(start[(i - 2) * boardSize + j - 1] == movingPlayer) {
								auto copy = start;
								copy.set((i - 2) * boardSize + j - 1);
								copy.spawn(i, j);
								*output = copy;
								++output;
								++num;
							}
						}
						if(start[(i - 2) * boardSize + j] == movingPlayer) {
							auto copy = start;
							copy.set((i - 2) * boardSize + j);
							copy.spawn(i, j);
							*output = copy;
							++output;
							++num;
						}
						if(j < boardSize - 1) {
							if(start[(i - 2) * boardSize + j + 1] == movingPlayer) {
								auto copy = start;
								copy.set((i - 2) * boardSize + j + 1);
								copy.spawn(i, j);
								*output = copy;
								++output;
								++num;
							}
							if(j < boardSize - 2 && start[(i - 2) * boardSize + j + 2] == movingPlayer) {
								auto copy = start;
								copy.set((i - 2) * boardSize + j + 2);
								copy.spawn(i, j);
								*output = copy;
								++output;
								++num;
							}
						}
					}
					if(i > 0) {
						// Check if a blob can jump in from one above and two to the left or right.
						if(j > 1 && start[(i - 1) * boardSize + j - 2] == movingPlayer) {
							auto copy = start;
							copy.set((i - 1) * boardSize + j - 2);
							copy.spawn(i, j);
							*output = copy;
							++output;
							++num;
						}
						if(j < boardSize - 2 && start[(i - 1) * boardSize + j + 2] == movingPlayer) {
							auto copy = start;
							copy.set((i - 1) * boardSize + j + 2);
							copy.spawn(i, j);
							*output = copy;
							++output;
							++num;
						}
					}
					// Check if a blob can jump in from two to the left or to the right.
					if(j > 1 && start[i * boardSize + j - 2] == movingPlayer) {
						auto copy = start;
						copy.set(i * boardSize + j - 2);
						copy.spawn(i, j);
						*output = copy;
						++output;
						++num;
					}
					if(j < boardSize - 2 && start[i * boardSize + j + 2] == movingPlayer) {
						auto copy = start;
						copy.set(i * boardSize + j + 2);
						copy.spawn(i, j);
						*output = copy;
						++output;
						++num;
					}
					if(i < boardSize - 1) {
						// Check if a blob can jump in from one row below and two to the left or right.
						if(j > 1 && start[(i + 1) * boardSize + j - 2] == movingPlayer) {
							auto copy = start;
							copy.set((i + 1) * boardSize + j - 2);
							copy.spawn(i, j);
							*output = copy;
							++output;
							++num;
						}
						if(j < boardSize - 2 && start[(i + 1) * boardSize + j + 2] == movingPlayer) {
							auto copy = start;
							copy.set((i + 1) * boardSize + j + 2);
							copy.spawn(i, j);
							*output = copy;
							++output;
							++num;
						}
					}
					if(i < boardSize - 2) {
						// Check if a blob can jump in from the row two below.
						if(j > 0) {
							if(j > 1 && start[(i + 2) * boardSize + j - 2] == movingPlayer) {
								auto copy = start;
								copy.set((i + 2) * boardSize + j - 2);
								copy.spawn(i, j);
								*output = copy;
								++output;
								++num;
							}
							if(start[(i + 2) * boardSize + j - 1] == movingPlayer) {
								auto copy = start;
								copy.set((i + 2) * boardSize + j - 1);
								copy.spawn(i, j);
								*output = copy;
								++output;
								++num;
							}
						}
						if(start[(i + 2) * boardSize + j] == movingPlayer) {
							auto copy = start;
							copy.set((i + 2) * boardSize + j);
							copy.spawn(i, j);
							*output = copy;
							++output;
							++num;
						}
						if(j < boardSize - 1) {
							if(start[(i + 2) * boardSize + j + 1] == movingPlayer) {
								auto copy = start;
								copy.set((i + 2) * boardSize + j + 1);
								copy.spawn(i, j);
								*output = copy;
								++output;
								++num;
							}
							if(j < boardSize - 2 && start[(i + 2) * boardSize + j + 2] == movingPlayer) {
								auto copy = start;
								copy.set((i + 2) * boardSize + j + 2);
								copy.spawn(i, j);
								*output = copy;
								++output;
								++num;
							}
						}
					}
				}
			}
			return num;
		}

	//! Apply the minimax algorithm to all moves below a given Status
	/*!
	 * Example call: minimax<4>(status, n)
	 * This will descend from status 4 levels deep, eveluating minimax, and save the next status to be assumed into n.
	 * n may point to status.
	 *
	 * \tparam depth The total depth that will be searched
	 * \param[in] status The Status we will explore further
	 * \param[in] nextStatus Pointer that the next status is written to in order to achieve this score, or nullptr if it shouldn't be written
	 * \param[in] level How far we descended into the tree, zero meaning that we're at the deepest level
	 * \return The score status receives, given that we descend level more levels into the tree
	 */
	template<uint32_t depth>
		Score minimax(Status const& status, Status * nextStatus = nullptr, const uint32_t level = depth, Score alpha = - static_cast<Score>(boardSize * boardSize), Score beta = static_cast<Score>(boardSize * boardSize))
		{
			if(level == 0) {
				return status.score();
			}

			// First determine all possible moves right now.
			std::array<Status, upperLimitMoves> moves;
			const auto len = generateStatuses(status, moves.begin());
			assert(len < moves.size());

			if(len == 0) {
				// There are no further moves to be explored.
				if(nextStatus != nullptr) {
					// So we change turns and return the same status since no move can be made.
					Status copy(status);
					copy.switchPlayerTurn();
					*nextStatus = std::move(copy);
				}
				// So the score of status is just that, its score.
				return status.score();
			}

			// Whether we are maximizing (or minimizing)
			const bool maximizing = status.whiteMoves();

			// We sort everything but the first level.
			// This heuristic proves to have the best performance.
			if(level != depth) {
				const auto pred = [level, maximizing] (const auto lhs, const auto rhs) {
					if(maximizing) {
						return lhs.score() > rhs.score();
					} else {
						return lhs.score() < rhs.score();
					}
				};
				std::sort(moves.begin(), moves.begin() + len, pred);
			}

			std::array<Score, upperLimitMoves> results;
			Score newScore = maximizing ? - static_cast<Score>(boardSize * boardSize) : static_cast<Score>(boardSize * boardSize);
			uint32_t newIndex = 0;
			// Now descend one further for each possible new status.
			for(uint32_t i = 0; i != len; ++i) {
				results[i] = minimax<depth>(moves[i], nullptr, level - 1, alpha, beta);
				if(maximizing) {
					if(results[i] > newScore) {
						newScore = results[i];
						newIndex = i;
					}
					if(results[i] > alpha) {
						alpha = results[i];
					}
					if(beta <= alpha) {
						break;
					}
				} else {
					if(results[i] < newScore) {
						newScore = results[i];
						newIndex = i;
					}
					if(results[i] < beta) {
						beta = results[i];
					}
					if(beta <= alpha) {
						break;
					}
				}
			}
			if(nextStatus != nullptr) {
				// Save the next status.
				*nextStatus = moves[newIndex];
			}
			return newScore;
		}

	//! Turn an index of a Status into human readable format
	inline std::string indexString(uint32_t index)
	{
		std::stringstream ss;
		assert(index < boardSize * boardSize);
		const auto row = index / boardSize;
		const auto col = index % boardSize;
		ss << static_cast<char>(col + 'A');
		ss << static_cast<char>(row + '0');
		return ss.str();
	}

	//! Print the move from one Status to another one in human readable form
	inline std::string moveString(Status const & first, Status const & second)
	{
		assert(first.whiteMoves() != second.whiteMoves());
		std::stringstream ss;

		uint32_t fromJump = static_cast<uint32_t>(-1);
		uint32_t newBlob = static_cast<uint32_t>(-1);
		for(uint32_t i = 0; i != boardSize * boardSize; ++i) {
			// See if there is somthing that became empty -- only possible if a jump occured.
			if(second[i] == Entry::Empty && first[i] != Entry::Empty) {
				fromJump = i;
			}
			if(first[i] == Entry::Empty && second[i] != Entry::Empty) {
				newBlob = i;
			}
		}
		assert(newBlob != static_cast<uint32_t>(-1));
		if(fromJump != static_cast<uint32_t>(-1)) {
			ss << indexString(fromJump);
		}
		ss << indexString(newBlob);
		return ss.str();
	}
}

#endif
