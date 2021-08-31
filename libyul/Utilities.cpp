/*
	This file is part of solidity.

	solidity is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	solidity is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with solidity.  If not, see <http://www.gnu.org/licenses/>.
*/
// SPDX-License-Identifier: GPL-3.0
/**
 * Some useful snippets for the optimiser.
 */

#include <libyul/Utilities.h>

#include <libyul/AST.h>
#include <libyul/Exceptions.h>

#include <libsolutil/CommonData.h>
#include <libsolutil/FixedHash.h>

#include <boost/algorithm/string.hpp>

#include <algorithm>
#include <sstream>
#include <vector>

using namespace std;
using namespace solidity;
using namespace solidity::yul;
using namespace solidity::util;

string solidity::yul::reindent(string const& _code)
{
	int constexpr indentationWidth = 4;

	auto constexpr static countBraces = [](string const& _s) noexcept -> int
	{
		auto const i = _s.find("//");
		auto const e = i == _s.npos ? end(_s) : next(begin(_s), static_cast<ptrdiff_t>(i));
		auto const opening = count_if(begin(_s), e, [](auto ch) { return ch == '{' || ch == '('; });
		auto const closing = count_if(begin(_s), e, [](auto ch) { return ch == '}' || ch == ')'; });
		return int(opening - closing);
	};

	vector<string> lines;
	boost::split(lines, _code, boost::is_any_of("\n"));
	for (string& line: lines)
		boost::trim(line);

	// Reduce multiple consecutive empty lines.
	lines = fold(lines, vector<string>{}, [](auto&& _lines, auto&& _line) {
		if (!(_line.empty() && !_lines.empty() && _lines.back().empty()))
			_lines.emplace_back(std::move(_line));
		return std::move(_lines);
	});

	stringstream out;
	int depth = 0;

	for (string const& line: lines)
	{
		int const diff = countBraces(line);
		if (diff < 0)
			depth += diff;

		if (!line.empty())
		{
			for (int i = 0; i < depth * indentationWidth; ++i)
				out << ' ';
			out << line;
		}
		out << '\n';

		if (diff > 0)
			depth += diff;
	}

	return out.str();
}

u256 solidity::yul::valueOfNumberLiteral(Literal const& _literal)
{
	yulAssert(_literal.kind == LiteralKind::Number, "Expected number literal!");

	std::string const& literalString = _literal.value.str();
	yulAssert(isValidDecimal(literalString) || isValidHex(literalString), "Invalid number literal!");
	return u256(literalString);
}

u256 solidity::yul::valueOfStringLiteral(Literal const& _literal)
{
	yulAssert(_literal.kind == LiteralKind::String, "Expected string literal!");
	yulAssert(_literal.value.str().size() <= 32, "Literal string too long!");

	return u256(h256(_literal.value.str(), h256::FromBinary, h256::AlignLeft));
}

u256 yul::valueOfBoolLiteral(Literal const& _literal)
{
	yulAssert(_literal.kind == LiteralKind::Boolean, "Expected bool literal!");

	if (_literal.value == "true"_yulstring)
		return u256(1);
	else if (_literal.value == "false"_yulstring)
		return u256(0);

	yulAssert(false, "Unexpected bool literal value!");
}

u256 solidity::yul::valueOfLiteral(Literal const& _literal)
{
	switch (_literal.kind)
	{
		case LiteralKind::Number:
			return valueOfNumberLiteral(_literal);
		case LiteralKind::Boolean:
			return valueOfBoolLiteral(_literal);
		case LiteralKind::String:
			return valueOfStringLiteral(_literal);
		default:
			yulAssert(false, "Unexpected literal kind!");
	}
}

template<>
bool Less<Literal>::operator()(Literal const& _lhs, Literal const& _rhs) const
{
	if (std::make_tuple(_lhs.kind, _lhs.type) != std::make_tuple(_rhs.kind, _rhs.type))
		return std::make_tuple(_lhs.kind, _lhs.type) < std::make_tuple(_rhs.kind, _rhs.type);

	if (_lhs.kind == LiteralKind::Number)
		return valueOfNumberLiteral(_lhs) < valueOfNumberLiteral(_rhs);
	else
		return _lhs.value < _rhs.value;
}

bool SwitchCaseCompareByLiteralValue::operator()(Case const* _lhs, Case const* _rhs) const
{
	yulAssert(_lhs && _rhs, "");
	return Less<Literal*>{}(_lhs->value.get(), _rhs->value.get());
}
