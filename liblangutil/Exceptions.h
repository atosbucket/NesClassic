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
 * @author Christian <c@ethdev.com>
 * @date 2014
 * Solidity exception hierarchy.
 */

#pragma once

#include <libsolutil/Exceptions.h>
#include <libsolutil/Assertions.h>
#include <libsolutil/CommonData.h>
#include <liblangutil/SourceLocation.h>

#include <string>
#include <utility>
#include <vector>
#include <memory>

namespace solidity::langutil
{
class Error;
using ErrorList = std::vector<std::shared_ptr<Error const>>;

struct CompilerError: virtual util::Exception {};
struct StackTooDeepError: virtual CompilerError {};
struct InternalCompilerError: virtual util::Exception {};
struct FatalError: virtual util::Exception {};
struct UnimplementedFeatureError: virtual util::Exception {};
struct InvalidAstError: virtual util::Exception {};

/// Assertion that throws an InternalCompilerError containing the given description if it is not met.
#define solAssert(CONDITION, DESCRIPTION) \
	assertThrow(CONDITION, ::solidity::langutil::InternalCompilerError, DESCRIPTION)

#define solUnimplementedAssert(CONDITION, DESCRIPTION) \
	assertThrow(CONDITION, ::solidity::langutil::UnimplementedFeatureError, DESCRIPTION)

#define solUnimplemented(DESCRIPTION) \
	solUnimplementedAssert(false, DESCRIPTION)

#define astAssert(CONDITION, DESCRIPTION) \
	assertThrow(CONDITION, ::solidity::langutil::InvalidAstError, DESCRIPTION)

using errorSourceLocationInfo = std::pair<std::string, SourceLocation>;

class SecondarySourceLocation
{
public:
	SecondarySourceLocation& append(std::string const& _errMsg, SourceLocation const& _sourceLocation)
	{
		infos.emplace_back(_errMsg, _sourceLocation);
		return *this;
	}
	SecondarySourceLocation& append(SecondarySourceLocation&& _other)
	{
		infos += std::move(_other.infos);
		return *this;
	}

	/// Limits the number of secondary source locations to 32 and appends a notice to the
	/// error message.
	void limitSize(std::string& _message)
	{
		size_t occurrences = infos.size();
		if (occurrences > 32)
		{
			infos.resize(32);
			_message += " Truncated from " + std::to_string(occurrences) + " to the first 32 occurrences.";
		}
	}

	std::vector<errorSourceLocationInfo> infos;
};

using errinfo_sourceLocation = boost::error_info<struct tag_sourceLocation, SourceLocation>;
using errinfo_secondarySourceLocation = boost::error_info<struct tag_secondarySourceLocation, SecondarySourceLocation>;

/**
 * Unique identifiers are used to tag and track individual error cases.
 * They are passed as the first parameter of error reporting functions.
 * Suffix _error helps to find them in the sources.
 * The struct ErrorId prevents incidental calls like typeError(3141) instead of typeError(3141_error).
 * To create a new ID, one can add 0000_error and then run "python ./scripts/error_codes.py --fix"
 * from the root of the repo.
 */
struct ErrorId
{
	unsigned long long error = 0;
	bool operator==(ErrorId const& _rhs) const { return error == _rhs.error; }
	bool operator!=(ErrorId const& _rhs) const { return !(*this == _rhs); }
};
constexpr ErrorId operator"" _error(unsigned long long _error) { return ErrorId{ _error }; }

class Error: virtual public util::Exception
{
public:
	enum class Type
	{
		CodeGenerationError,
		DeclarationError,
		DocstringParsingError,
		ParserError,
		TypeError,
		SyntaxError,
		Warning
	};

	Error(
		ErrorId _errorId,
		Type _type,
		std::string const& _description,
		SourceLocation const& _location = SourceLocation(),
		SecondarySourceLocation const& _secondaryLocation = SecondarySourceLocation()
	);

	ErrorId errorId() const { return m_errorId; }
	Type type() const { return m_type; }
	std::string const& typeName() const { return m_typeName; }

	/// helper functions
	static Error const* containsErrorOfType(ErrorList const& _list, Error::Type _type)
	{
		for (auto e: _list)
		{
			if (e->type() == _type)
				return e.get();
		}
		return nullptr;
	}
	static bool containsOnlyWarnings(ErrorList const& _list)
	{
		for (auto e: _list)
		{
			if (e->type() != Type::Warning)
				return false;
		}
		return true;
	}
private:
	ErrorId m_errorId;
	Type m_type;
	std::string m_typeName;
};

}
