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

#include <liblangutil/CharStreamProvider.h>
#include <liblangutil/Exceptions.h>
#include <libsolidity/ast/AST.h>
#include <libsolidity/lsp/FileRepository.h>
#include <libsolidity/lsp/Utils.h>

#include <fmt/format.h>
#include <fstream>

namespace solidity::lsp
{

using namespace frontend;
using namespace langutil;
using namespace std;

optional<LineColumn> parseLineColumn(Json::Value const& _lineColumn)
{
	if (_lineColumn.isObject() && _lineColumn["line"].isInt() && _lineColumn["character"].isInt())
		return LineColumn{_lineColumn["line"].asInt(), _lineColumn["character"].asInt()};
	else
		return nullopt;
}

Json::Value toJson(LineColumn const& _pos)
{
	Json::Value json = Json::objectValue;
	json["line"] = max(_pos.line, 0);
	json["character"] = max(_pos.column, 0);

	return json;
}

Json::Value toJsonRange(LineColumn const& _start, LineColumn const& _end)
{
	Json::Value json;
	json["start"] = toJson(_start);
	json["end"] = toJson(_end);
	return json;
}

Declaration const* referencedDeclaration(Expression const* _expression)
{
	if (auto const* identifier = dynamic_cast<Identifier const*>(_expression))
		if (Declaration const* referencedDeclaration = identifier->annotation().referencedDeclaration)
			return referencedDeclaration;

	if (auto const* memberAccess = dynamic_cast<MemberAccess const*>(_expression))
		if (memberAccess->annotation().referencedDeclaration)
			return memberAccess->annotation().referencedDeclaration;

	return nullptr;
}

optional<SourceLocation> declarationLocation(Declaration const* _declaration)
{
	if (!_declaration)
		return nullopt;

	if (_declaration->nameLocation().isValid())
		return _declaration->nameLocation();

	if (_declaration->location().isValid())
		return _declaration->location();

	return nullopt;
}

optional<SourceLocation> parsePosition(
	FileRepository const& _fileRepository,
	string const& _sourceUnitName,
	Json::Value const& _position
)
{
	if (!_fileRepository.sourceUnits().count(_sourceUnitName))
		return nullopt;

	if (optional<LineColumn> lineColumn = parseLineColumn(_position))
		if (optional<int> const offset = CharStream::translateLineColumnToPosition(
			_fileRepository.sourceUnits().at(_sourceUnitName),
			*lineColumn
		))
			return SourceLocation{*offset, *offset, make_shared<string>(_sourceUnitName)};
	return nullopt;
}

optional<SourceLocation> parseRange(FileRepository const& _fileRepository, string const& _sourceUnitName, Json::Value const& _range)
{
	if (!_range.isObject())
		return nullopt;
	optional<SourceLocation> start = parsePosition(_fileRepository, _sourceUnitName, _range["start"]);
	optional<SourceLocation> end = parsePosition(_fileRepository, _sourceUnitName, _range["end"]);
	if (!start || !end)
		return nullopt;
	solAssert(*start->sourceName == *end->sourceName);
	start->end = end->end;
	return start;
}

}
