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
 * @date 2016
 * Solidity parser shared functionality.
 */

#pragma once

#include <liblangutil/Token.h>
#include <memory>
#include <string>

namespace solidity::langutil
{

class ErrorReporter;
class Scanner;
struct SourceLocation;
struct ErrorId;

class ParserBase
{
public:
	/// Set @a _parserErrorRecovery to true for additional error
	/// recovery.  This is experimental and intended for use
	/// by front-end tools that need partial AST information even
	/// when errors occur.
	explicit ParserBase(ErrorReporter& errorReporter, bool _parserErrorRecovery = false): m_errorReporter(errorReporter)
	{
		m_parserErrorRecovery = _parserErrorRecovery;
	}

	virtual ~ParserBase() = default;

protected:
	/// Utility class that creates an error and throws an exception if the
	/// recursion depth is too deep.
	class RecursionGuard
	{
	public:
		explicit RecursionGuard(ParserBase& _parser): m_parser(_parser)
		{
			m_parser.increaseRecursionDepth();
		}
		~RecursionGuard() { m_parser.decreaseRecursionDepth(); }
	private:
		ParserBase& m_parser;
	};

	/// Location of the current token
	virtual SourceLocation currentLocation() const;

	///@{
	///@name Helper functions
	/// If current token value is not @a _value, throw exception otherwise advance token
	//  @a if _advance is true and error recovery is in effect.
	void expectToken(Token _value, bool _advance = true);

	/// Like expectToken but if there is an error ignores tokens until
	/// the expected token or EOS is seen. If EOS is encountered, back up to the error point,
	/// and throw an exception so that a higher grammar rule has an opportunity to recover.
	void expectTokenOrConsumeUntil(Token _value, std::string const& _currentNodeName, bool _advance = true);
	Token currentToken() const;
	Token peekNextToken() const;
	std::string tokenName(Token _token);
	std::string currentLiteral() const;
	virtual Token advance();
	///@}

	/// Increases the recursion depth and throws an exception if it is too deep.
	void increaseRecursionDepth();
	void decreaseRecursionDepth();

	/// Creates a @ref ParserError and annotates it with the current position and the
	/// given @a _description.
	void parserError(ErrorId _error, std::string const& _description);
	void parserError(ErrorId _error, SourceLocation const& _location, std::string const& _description);

	/// Creates a @ref ParserWarning and annotates it with the current position and the
	/// given @a _description.
	void parserWarning(ErrorId _error, std::string const& _description);
	void parserWarning(ErrorId _error, SourceLocation const& _location, std::string const& _description);

	/// Creates a @ref ParserError and annotates it with the current position and the
	/// given @a _description. Throws the FatalError.
	void fatalParserError(ErrorId _error, std::string const& _description);
	void fatalParserError(ErrorId _error, SourceLocation const& _location, std::string const& _description);

	std::shared_ptr<Scanner> m_scanner;
	/// The reference to the list of errors, warnings and infos to add errors/warnings/infos during parsing
	ErrorReporter& m_errorReporter;
	/// Current recursion depth during parsing.
	size_t m_recursionDepth = 0;
	/// True if we are in parser error recovery. Usually this means we are scanning for
	/// a synchronization token like ';', or '}'. We use this to reduce cascaded error messages.
	bool m_inParserRecovery = false;
	bool m_parserErrorRecovery = false;
};

}
