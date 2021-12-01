/*
 * This file is part of solidity.
 *
 * solidity is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * solidity is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with solidity.  If not, see <http://www.gnu.org/licenses/>.
 *
 * This file is derived from the file "scanner.h", which was part of the
 * V8 project. The original copyright header follows:
 *
 * Copyright 2006-2012, the V8 project authors. All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer in the documentation and/or other materials provided
 *   with the distribution.
 * * Neither the name of Google Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/**
 * Character stream / input file.
 */

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <utility>

namespace solidity::langutil
{

struct SourceLocation;
struct LineColumn;

/**
 * Bidirectional stream of characters.
 *
 * This CharStream is used by lexical analyzers as the source.
 */
class CharStream
{
public:
	CharStream() = default;
	CharStream(std::string _source, std::string _name):
		m_source(std::move(_source)), m_name(std::move(_name)) {}

	size_t position() const { return m_position; }
	bool isPastEndOfInput(size_t _charsForward = 0) const { return (m_position + _charsForward) >= m_source.size(); }

	char get(size_t _charsForward = 0) const { return m_source[m_position + _charsForward]; }
	char advanceAndGet(size_t _chars = 1);
	/// Sets scanner position to @ _amount characters backwards in source text.
	/// @returns The character of the current location after update is returned.
	char rollback(size_t _amount);
	/// Sets scanner position to @ _location if it refers a valid offset in m_source.
	/// If not, nothing is done.
	/// @returns The character of the current location after update is returned.
	char setPosition(size_t _location);

	void reset() { m_position = 0; }

	std::string const& source() const noexcept { return m_source; }
	std::string const& name() const noexcept { return m_name; }

	size_t size() const { return m_source.size(); }

	///@{
	///@name Error printing helper functions
	/// Functions that help pretty-printing parse errors
	/// Do only use in error cases, they are quite expensive.
	std::string lineAtPosition(int _position) const;
	LineColumn translatePositionToLineColumn(int _position) const;
	///@}

	/// Translates a line:column to the absolute position.
	std::optional<int> translateLineColumnToPosition(LineColumn const& _lineColumn) const;

	/// Translates a line:column to the absolute position for the given input text.
	static std::optional<int> translateLineColumnToPosition(std::string const& _text, LineColumn const& _input);

	/// Tests whether or not given octet sequence is present at the current position in stream.
	/// @returns true if the sequence could be found, false otherwise.
	bool prefixMatch(std::string_view _sequence)
	{
		if (isPastEndOfInput(_sequence.size()))
			return false;

		for (size_t i = 0; i < _sequence.size(); ++i)
			if (_sequence[i] != get(i))
				return false;

		return true;
	}

	/// @returns the substring of the source that the source location references.
	/// Returns an empty string view if the source location does not `hasText()`.
	std::string_view text(SourceLocation const& _location) const;

	/// @returns the first line of the referenced source fragment. If the fragment is longer than
	/// one line, appends an ellipsis to indicate that.
	std::string singleLineSnippet(SourceLocation const& _location) const
	{
		return singleLineSnippet(m_source, _location);
	}

	static std::string singleLineSnippet(std::string const& _sourceCode, SourceLocation const& _location);

private:
	std::string m_source;
	std::string m_name;
	size_t m_position{0};
};

}
