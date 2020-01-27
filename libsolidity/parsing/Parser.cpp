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
/**
 * @author Christian <c@ethdev.com>
 * @date 2014
 * Solidity parser.
 */

#include <libsolidity/parsing/Parser.h>

#include <libsolidity/interface/Version.h>
#include <libyul/AsmParser.h>
#include <libyul/backends/evm/EVMDialect.h>
#include <liblangutil/ErrorReporter.h>
#include <liblangutil/Scanner.h>
#include <liblangutil/SemVerHandler.h>
#include <liblangutil/SourceLocation.h>
#include <libyul/backends/evm/EVMDialect.h>
#include <cctype>
#include <vector>

using namespace std;
using namespace solidity::langutil;

namespace solidity::frontend
{

/// AST node factory that also tracks the begin and end position of an AST node
/// while it is being parsed
class Parser::ASTNodeFactory
{
public:
	explicit ASTNodeFactory(Parser& _parser):
		m_parser(_parser), m_location{_parser.position(), -1, _parser.source()} {}
	ASTNodeFactory(Parser& _parser, ASTPointer<ASTNode> const& _childNode):
		m_parser(_parser), m_location{_childNode->location()} {}

	void markEndPosition() { m_location.end = m_parser.endPosition(); }
	void setLocation(SourceLocation const& _location) { m_location = _location; }
	void setLocationEmpty() { m_location.end = m_location.start; }
	/// Set the end position to the one of the given node.
	void setEndPositionFromNode(ASTPointer<ASTNode> const& _node) { m_location.end = _node->location().end; }

	template <class NodeType, typename... Args>
	ASTPointer<NodeType> createNode(Args&& ... _args)
	{
		solAssert(m_location.source, "");
		if (m_location.end < 0)
			markEndPosition();
		return make_shared<NodeType>(m_parser.nextID(), m_location, std::forward<Args>(_args)...);
	}

	SourceLocation const& location() const noexcept { return m_location; }

private:
	Parser& m_parser;
	SourceLocation m_location;
};

ASTPointer<SourceUnit> Parser::parse(shared_ptr<Scanner> const& _scanner)
{
	solAssert(!m_insideModifier, "");
	try
	{
		m_recursionDepth = 0;
		m_scanner = _scanner;
		ASTNodeFactory nodeFactory(*this);
		vector<ASTPointer<ASTNode>> nodes;
		while (m_scanner->currentToken() != Token::EOS)
		{
			switch (m_scanner->currentToken())
			{
			case Token::Pragma:
				nodes.push_back(parsePragmaDirective());
				break;
			case Token::Import:
				nodes.push_back(parseImportDirective());
				break;
			case Token::Abstract:
			case Token::Interface:
			case Token::Contract:
			case Token::Library:
				nodes.push_back(parseContractDefinition());
				break;
			case Token::Struct:
				nodes.push_back(parseStructDefinition());
				break;
			case Token::Enum:
				nodes.push_back(parseEnumDefinition());
				break;
			default:
				fatalParserError(string("Expected pragma, import directive or contract/interface/library/struct/enum definition."));
			}
		}
		solAssert(m_recursionDepth == 0, "");
		return nodeFactory.createNode<SourceUnit>(nodes);
	}
	catch (FatalError const&)
	{
		if (m_errorReporter.errors().empty())
			throw; // Something is weird here, rather throw again.
		return nullptr;
	}
}

void Parser::parsePragmaVersion(SourceLocation const& _location, vector<Token> const& _tokens, vector<string> const& _literals)
{
	SemVerMatchExpressionParser parser(_tokens, _literals);
	auto matchExpression = parser.parse();
	static SemVerVersion const currentVersion{string(VersionString)};
	// FIXME: only match for major version incompatibility
	if (!matchExpression.matches(currentVersion))
		// If m_parserErrorRecovery is true, the same message will appear from SyntaxChecker::visit(),
		// so we don't need to report anything here.
		if (!m_parserErrorRecovery)
			m_errorReporter.fatalParserError(
				_location,
				"Source file requires different compiler version (current compiler is " +
				string(VersionString) + " - note that nightly builds are considered to be "
				"strictly less than the released version"
			);
}

ASTPointer<PragmaDirective> Parser::parsePragmaDirective()
{
	RecursionGuard recursionGuard(*this);
	// pragma anything* ;
	// Currently supported:
	// pragma solidity ^0.4.0 || ^0.3.0;
	ASTNodeFactory nodeFactory(*this);
	expectToken(Token::Pragma);
	vector<string> literals;
	vector<Token> tokens;
	do
	{
		Token token = m_scanner->currentToken();
		if (token == Token::Illegal)
			parserError("Token incompatible with Solidity parser as part of pragma directive.");
		else
		{
			string literal = m_scanner->currentLiteral();
			if (literal.empty() && TokenTraits::toString(token))
				literal = TokenTraits::toString(token);
			literals.push_back(literal);
			tokens.push_back(token);
		}
		m_scanner->next();
	}
	while (m_scanner->currentToken() != Token::Semicolon && m_scanner->currentToken() != Token::EOS);
	nodeFactory.markEndPosition();
	expectToken(Token::Semicolon);

	if (literals.size() >= 2 && literals[0] == "solidity")
	{
		parsePragmaVersion(
			nodeFactory.location(),
			vector<Token>(tokens.begin() + 1, tokens.end()),
			vector<string>(literals.begin() + 1, literals.end())
		);
	}

	return nodeFactory.createNode<PragmaDirective>(tokens, literals);
}

ASTPointer<ImportDirective> Parser::parseImportDirective()
{
	RecursionGuard recursionGuard(*this);
	// import "abc" [as x];
	// import * as x from "abc";
	// import {a as b, c} from "abc";
	ASTNodeFactory nodeFactory(*this);
	expectToken(Token::Import);
	ASTPointer<ASTString> path;
	ASTPointer<ASTString> unitAlias = make_shared<string>();
	ImportDirective::SymbolAliasList symbolAliases;

	if (m_scanner->currentToken() == Token::StringLiteral)
	{
		path = getLiteralAndAdvance();
		if (m_scanner->currentToken() == Token::As)
		{
			m_scanner->next();
			unitAlias = expectIdentifierToken();
		}
	}
	else
	{
		if (m_scanner->currentToken() == Token::LBrace)
		{
			m_scanner->next();
			while (true)
			{
				ASTPointer<ASTString> alias;
				SourceLocation aliasLocation = SourceLocation{position(), endPosition(), source()};
				ASTPointer<Identifier> id = parseIdentifier();
				if (m_scanner->currentToken() == Token::As)
				{
					expectToken(Token::As);
					aliasLocation = SourceLocation{position(), endPosition(), source()};
					alias = expectIdentifierToken();
				}
				symbolAliases.emplace_back(ImportDirective::SymbolAlias{move(id), move(alias), aliasLocation});
				if (m_scanner->currentToken() != Token::Comma)
					break;
				m_scanner->next();
			}
			expectToken(Token::RBrace);
		}
		else if (m_scanner->currentToken() == Token::Mul)
		{
			m_scanner->next();
			expectToken(Token::As);
			unitAlias = expectIdentifierToken();
		}
		else
			fatalParserError("Expected string literal (path), \"*\" or alias list.");
		// "from" is not a keyword but parsed as an identifier because of backwards
		// compatibility and because it is a really common word.
		if (m_scanner->currentToken() != Token::Identifier || m_scanner->currentLiteral() != "from")
			fatalParserError("Expected \"from\".");
		m_scanner->next();
		if (m_scanner->currentToken() != Token::StringLiteral)
			fatalParserError("Expected import path.");
		path = getLiteralAndAdvance();
	}
	if (path->empty())
		fatalParserError("Import path cannot be empty.");
	nodeFactory.markEndPosition();
	expectToken(Token::Semicolon);
	return nodeFactory.createNode<ImportDirective>(path, unitAlias, move(symbolAliases));
}

std::pair<ContractKind, bool> Parser::parseContractKind()
{
	ContractKind kind;
	bool abstract = false;
	if (m_scanner->currentToken() == Token::Abstract)
	{
		abstract = true;
		m_scanner->next();
	}
	switch (m_scanner->currentToken())
	{
	case Token::Interface:
		kind = ContractKind::Interface;
		break;
	case Token::Contract:
		kind = ContractKind::Contract;
		break;
	case Token::Library:
		kind = ContractKind::Library;
		break;
	default:
		solAssert(false, "Invalid contract kind.");
	}
	m_scanner->next();
	return std::make_pair(kind, abstract);
}

ASTPointer<ContractDefinition> Parser::parseContractDefinition()
{
	RecursionGuard recursionGuard(*this);
	ASTNodeFactory nodeFactory(*this);
	ASTPointer<ASTString> name =  nullptr;
	ASTPointer<ASTString> docString;
	vector<ASTPointer<InheritanceSpecifier>> baseContracts;
	vector<ASTPointer<ASTNode>> subNodes;
	std::pair<ContractKind, bool> contractKind{};
	try
	{
		if (m_scanner->currentCommentLiteral() != "")
			docString = make_shared<ASTString>(m_scanner->currentCommentLiteral());
		contractKind = parseContractKind();
		name = expectIdentifierToken();
		if (m_scanner->currentToken() == Token::Is)
			do
			{
				m_scanner->next();
				baseContracts.push_back(parseInheritanceSpecifier());
			}
			while (m_scanner->currentToken() == Token::Comma);
		expectToken(Token::LBrace);
		while (true)
		{
			Token currentTokenValue = m_scanner->currentToken();
			if (currentTokenValue == Token::RBrace)
				break;
			else if (
				(currentTokenValue == Token::Function && m_scanner->peekNextToken() != Token::LParen) ||
				currentTokenValue == Token::Constructor ||
				currentTokenValue == Token::Receive ||
				currentTokenValue == Token::Fallback
			)
				subNodes.push_back(parseFunctionDefinition());
			else if (currentTokenValue == Token::Struct)
				subNodes.push_back(parseStructDefinition());
			else if (currentTokenValue == Token::Enum)
				subNodes.push_back(parseEnumDefinition());
			else if (
				currentTokenValue == Token::Identifier ||
				currentTokenValue == Token::Mapping ||
				TokenTraits::isElementaryTypeName(currentTokenValue) ||
				(currentTokenValue == Token::Function && m_scanner->peekNextToken() == Token::LParen)
			)
			{
				VarDeclParserOptions options;
				options.isStateVariable = true;
				options.allowInitialValue = true;
				subNodes.push_back(parseVariableDeclaration(options));
				expectToken(Token::Semicolon);
			}
			else if (currentTokenValue == Token::Modifier)
				subNodes.push_back(parseModifierDefinition());
			else if (currentTokenValue == Token::Event)
				subNodes.push_back(parseEventDefinition());
			else if (currentTokenValue == Token::Using)
				subNodes.push_back(parseUsingDirective());
			else
				fatalParserError(string("Function, variable, struct or modifier declaration expected."));
		}
	}
	catch (FatalError const&)
	{
		if (
			!m_errorReporter.hasErrors() ||
			!m_parserErrorRecovery ||
			m_errorReporter.hasExcessiveErrors()
		)
			BOOST_THROW_EXCEPTION(FatalError()); /* Don't try to recover here. */
		m_inParserRecovery = true;
	}
	nodeFactory.markEndPosition();
	if (m_inParserRecovery)
		expectTokenOrConsumeUntil(Token::RBrace, "ContractDefinition");
	else
		expectToken(Token::RBrace);
	return nodeFactory.createNode<ContractDefinition>(
		name,
		docString,
		baseContracts,
		subNodes,
		contractKind.first,
		contractKind.second
	);
}

ASTPointer<InheritanceSpecifier> Parser::parseInheritanceSpecifier()
{
	RecursionGuard recursionGuard(*this);
	ASTNodeFactory nodeFactory(*this);
	ASTPointer<UserDefinedTypeName> name(parseUserDefinedTypeName());
	unique_ptr<vector<ASTPointer<Expression>>> arguments;
	if (m_scanner->currentToken() == Token::LParen)
	{
		m_scanner->next();
		arguments = make_unique<vector<ASTPointer<Expression>>>(parseFunctionCallListArguments());
		nodeFactory.markEndPosition();
		expectToken(Token::RParen);
	}
	else
		nodeFactory.setEndPositionFromNode(name);
	return nodeFactory.createNode<InheritanceSpecifier>(name, std::move(arguments));
}

Visibility Parser::parseVisibilitySpecifier()
{
	Visibility visibility(Visibility::Default);
	Token token = m_scanner->currentToken();
	switch (token)
	{
		case Token::Public:
			visibility = Visibility::Public;
			break;
		case Token::Internal:
			visibility = Visibility::Internal;
			break;
		case Token::Private:
			visibility = Visibility::Private;
			break;
		case Token::External:
			visibility = Visibility::External;
			break;
		default:
			solAssert(false, "Invalid visibility specifier.");
	}
	m_scanner->next();
	return visibility;
}

ASTPointer<OverrideSpecifier> Parser::parseOverrideSpecifier()
{
	solAssert(m_scanner->currentToken() == Token::Override, "");

	ASTNodeFactory nodeFactory(*this);
	std::vector<ASTPointer<UserDefinedTypeName>> overrides;

	nodeFactory.markEndPosition();
	m_scanner->next();

	if (m_scanner->currentToken() == Token::LParen)
	{
		m_scanner->next();
		while (true)
		{
			overrides.push_back(parseUserDefinedTypeName());

			if (m_scanner->currentToken() == Token::RParen)
				break;

			expectToken(Token::Comma);
		}

		nodeFactory.markEndPosition();
		expectToken(Token::RParen);
	}

	return nodeFactory.createNode<OverrideSpecifier>(move(overrides));
}

StateMutability Parser::parseStateMutability()
{
	StateMutability stateMutability(StateMutability::NonPayable);
	Token token = m_scanner->currentToken();
	switch (token)
	{
		case Token::Payable:
			stateMutability = StateMutability::Payable;
			break;
		case Token::View:
			stateMutability = StateMutability::View;
			break;
		case Token::Pure:
			stateMutability = StateMutability::Pure;
			break;
		case Token::Constant:
			stateMutability = StateMutability::View;
			parserError(
				"The state mutability modifier \"constant\" was removed in version 0.5.0. "
				"Use \"view\" or \"pure\" instead."
			);
			break;
		default:
			solAssert(false, "Invalid state mutability specifier.");
	}
	m_scanner->next();
	return stateMutability;
}

Parser::FunctionHeaderParserResult Parser::parseFunctionHeader(bool _isStateVariable)
{
	RecursionGuard recursionGuard(*this);
	FunctionHeaderParserResult result;

	VarDeclParserOptions options;
	options.allowLocationSpecifier = true;
	result.parameters = parseParameterList(options);
	while (true)
	{
		Token token = m_scanner->currentToken();
		if (!_isStateVariable && token == Token::Identifier)
			result.modifiers.push_back(parseModifierInvocation());
		else if (TokenTraits::isVisibilitySpecifier(token))
		{
			if (result.visibility != Visibility::Default)
			{
				// There is the special case of a public state variable of function type.
				// Detect this and return early.
				if (_isStateVariable && (result.visibility == Visibility::External || result.visibility == Visibility::Internal))
					break;
				parserError(string(
					"Visibility already specified as \"" +
					Declaration::visibilityToString(result.visibility) +
					"\"."
				));
				m_scanner->next();
			}
			else
				result.visibility = parseVisibilitySpecifier();
		}
		else if (TokenTraits::isStateMutabilitySpecifier(token))
		{
			if (result.stateMutability != StateMutability::NonPayable)
			{
				parserError(string(
					"State mutability already specified as \"" +
					stateMutabilityToString(result.stateMutability) +
					"\"."
				));
				m_scanner->next();
			}
			else
				result.stateMutability = parseStateMutability();
		}
		else if (!_isStateVariable && token == Token::Override)
		{
			if (result.overrides)
				parserError("Override already specified.");

			result.overrides = parseOverrideSpecifier();
		}
		else if (!_isStateVariable && token == Token::Virtual)
		{
			if (result.isVirtual)
				parserError("Virtual already specified.");

			result.isVirtual = true;
			m_scanner->next();
		}
		else
			break;
	}
	if (m_scanner->currentToken() == Token::Returns)
	{
		bool const permitEmptyParameterList = false;
		m_scanner->next();
		result.returnParameters = parseParameterList(options, permitEmptyParameterList);
	}
	else
		result.returnParameters = createEmptyParameterList();
	return result;
}

ASTPointer<ASTNode> Parser::parseFunctionDefinition()
{
	RecursionGuard recursionGuard(*this);
	ASTNodeFactory nodeFactory(*this);
	ASTPointer<ASTString> docstring;
	if (m_scanner->currentCommentLiteral() != "")
		docstring = make_shared<ASTString>(m_scanner->currentCommentLiteral());

	Token kind = m_scanner->currentToken();
	ASTPointer<ASTString> name;
	if (kind == Token::Function)
	{
		m_scanner->next();
		if (
			m_scanner->currentToken() == Token::Constructor ||
			m_scanner->currentToken() == Token::Fallback ||
			m_scanner->currentToken() == Token::Receive
		)
		{
			std::string expected = std::map<Token, std::string>{
				{Token::Constructor, "constructor"},
				{Token::Fallback, "fallback function"},
				{Token::Receive, "receive function"},
			}.at(m_scanner->currentToken());
			name = make_shared<ASTString>(TokenTraits::toString(m_scanner->currentToken()));
			string message{
				"This function is named \"" + *name + "\" but is not the " + expected + " of the contract. "
				"If you intend this to be a " + expected + ", use \"" + *name + "(...) { ... }\" without "
				"the \"function\" keyword to define it."
			};
			if (m_scanner->currentToken() == Token::Constructor)
				parserError(message);
			else
				parserWarning(message);
			m_scanner->next();
		}
		else
			name = expectIdentifierToken();
	}
	else
	{
		solAssert(kind == Token::Constructor || kind == Token::Fallback || kind == Token::Receive, "");
		m_scanner->next();
		name = make_shared<ASTString>();
	}

	FunctionHeaderParserResult header = parseFunctionHeader(false);

	ASTPointer<Block> block;
	nodeFactory.markEndPosition();
	if (m_scanner->currentToken() != Token::Semicolon)
	{
		block = parseBlock();
		nodeFactory.setEndPositionFromNode(block);
	}
	else
		m_scanner->next(); // just consume the ';'
	return nodeFactory.createNode<FunctionDefinition>(
		name,
		header.visibility,
		header.stateMutability,
		kind,
		header.isVirtual,
		header.overrides,
		docstring,
		header.parameters,
		header.modifiers,
		header.returnParameters,
		block
	);
}

ASTPointer<StructDefinition> Parser::parseStructDefinition()
{
	RecursionGuard recursionGuard(*this);
	ASTNodeFactory nodeFactory(*this);
	expectToken(Token::Struct);
	ASTPointer<ASTString> name = expectIdentifierToken();
	vector<ASTPointer<VariableDeclaration>> members;
	expectToken(Token::LBrace);
	while (m_scanner->currentToken() != Token::RBrace)
	{
		members.push_back(parseVariableDeclaration());
		expectToken(Token::Semicolon);
	}
	nodeFactory.markEndPosition();
	expectToken(Token::RBrace);
	return nodeFactory.createNode<StructDefinition>(name, members);
}

ASTPointer<EnumValue> Parser::parseEnumValue()
{
	RecursionGuard recursionGuard(*this);
	ASTNodeFactory nodeFactory(*this);
	nodeFactory.markEndPosition();
	return nodeFactory.createNode<EnumValue>(expectIdentifierToken());
}

ASTPointer<EnumDefinition> Parser::parseEnumDefinition()
{
	RecursionGuard recursionGuard(*this);
	ASTNodeFactory nodeFactory(*this);
	expectToken(Token::Enum);
	ASTPointer<ASTString> name = expectIdentifierToken();
	vector<ASTPointer<EnumValue>> members;
	expectToken(Token::LBrace);

	while (m_scanner->currentToken() != Token::RBrace)
	{
		members.push_back(parseEnumValue());
		if (m_scanner->currentToken() == Token::RBrace)
			break;
		expectToken(Token::Comma);
		if (m_scanner->currentToken() != Token::Identifier)
			fatalParserError(string("Expected identifier after ','"));
	}
	if (members.empty())
		parserError({"enum with no members is not allowed."});

	nodeFactory.markEndPosition();
	expectToken(Token::RBrace);
	return nodeFactory.createNode<EnumDefinition>(name, members);
}

ASTPointer<VariableDeclaration> Parser::parseVariableDeclaration(
	VarDeclParserOptions const& _options,
	ASTPointer<TypeName> const& _lookAheadArrayType
)
{
	RecursionGuard recursionGuard(*this);
	ASTNodeFactory nodeFactory = _lookAheadArrayType ?
		ASTNodeFactory(*this, _lookAheadArrayType) : ASTNodeFactory(*this);
	ASTPointer<TypeName> type;
	if (_lookAheadArrayType)
		type = _lookAheadArrayType;
	else
	{
		type = parseTypeName(_options.allowVar);
		if (type != nullptr)
			nodeFactory.setEndPositionFromNode(type);
	}

	if (dynamic_cast<FunctionTypeName*>(type.get()) && _options.isStateVariable && m_scanner->currentToken() == Token::LBrace)
		fatalParserError(
			"Expected a state variable declaration. If you intended this as a fallback function "
			"or a function to handle plain ether transactions, use the \"fallback\" keyword "
			"or the \"receive\" keyword instead."
		);

	bool isIndexed = false;
	bool isDeclaredConst = false;
	ASTPointer<OverrideSpecifier> overrides = nullptr;
	Visibility visibility(Visibility::Default);
	VariableDeclaration::Location location = VariableDeclaration::Location::Unspecified;
	ASTPointer<ASTString> identifier;

	while (true)
	{
		Token token = m_scanner->currentToken();
		if (_options.isStateVariable && TokenTraits::isVariableVisibilitySpecifier(token))
		{
			nodeFactory.markEndPosition();
			if (visibility != Visibility::Default)
			{
				parserError(string(
					"Visibility already specified as \"" +
					Declaration::visibilityToString(visibility) +
					"\"."
				));
				m_scanner->next();
			}
			else
				visibility = parseVisibilitySpecifier();
		}
		else if (_options.isStateVariable && token == Token::Override)
		{
			if (overrides)
				parserError("Override already specified.");

			overrides = parseOverrideSpecifier();
		}
		else
		{
			if (_options.allowIndexed && token == Token::Indexed)
				isIndexed = true;
			else if (token == Token::Constant)
				isDeclaredConst = true;
			else if (_options.allowLocationSpecifier && TokenTraits::isLocationSpecifier(token))
			{
				if (location != VariableDeclaration::Location::Unspecified)
					parserError(string("Location already specified."));
				else if (!type)
					parserError(string("Location specifier needs explicit type name."));
				else
				{
					switch (token)
					{
					case Token::Storage:
						location = VariableDeclaration::Location::Storage;
						break;
					case Token::Memory:
						location = VariableDeclaration::Location::Memory;
						break;
					case Token::CallData:
						location = VariableDeclaration::Location::CallData;
						break;
					default:
						solAssert(false, "Unknown data location.");
					}
				}
			}
			else
				break;
			nodeFactory.markEndPosition();
			m_scanner->next();
		}
	}

	if (_options.allowEmptyName && m_scanner->currentToken() != Token::Identifier)
	{
		identifier = make_shared<ASTString>("");
		solAssert(!_options.allowVar, ""); // allowEmptyName && allowVar makes no sense
	}
	else
	{
		nodeFactory.markEndPosition();
		identifier = expectIdentifierToken();
	}
	ASTPointer<Expression> value;
	if (_options.allowInitialValue)
	{
		if (m_scanner->currentToken() == Token::Assign)
		{
			m_scanner->next();
			value = parseExpression();
			nodeFactory.setEndPositionFromNode(value);
		}
	}
	return nodeFactory.createNode<VariableDeclaration>(
		type,
		identifier,
		value,
		visibility,
		_options.isStateVariable,
		isIndexed,
		isDeclaredConst,
		overrides,
		location
	);
}

ASTPointer<ModifierDefinition> Parser::parseModifierDefinition()
{
	RecursionGuard recursionGuard(*this);
	ScopeGuard resetModifierFlag([this]() { m_insideModifier = false; });
	m_insideModifier = true;

	ASTNodeFactory nodeFactory(*this);
	ASTPointer<ASTString> docstring;
	if (m_scanner->currentCommentLiteral() != "")
		docstring = make_shared<ASTString>(m_scanner->currentCommentLiteral());

	expectToken(Token::Modifier);
	ASTPointer<ASTString> name(expectIdentifierToken());
	ASTPointer<ParameterList> parameters;
	if (m_scanner->currentToken() == Token::LParen)
	{
		VarDeclParserOptions options;
		options.allowIndexed = true;
		options.allowLocationSpecifier = true;
		parameters = parseParameterList(options);
	}
	else
		parameters = createEmptyParameterList();

	ASTPointer<OverrideSpecifier> overrides;
	bool isVirtual = false;

	while (true)
	{
		if (m_scanner->currentToken() == Token::Override)
		{
			if (overrides)
				parserError("Override already specified.");
			overrides = parseOverrideSpecifier();
		}
		else if (m_scanner->currentToken() == Token::Virtual)
		{
			if (isVirtual)
				parserError("Virtual already specified.");

			isVirtual = true;
			m_scanner->next();
		}
		else
			break;
	}


	ASTPointer<Block> block = parseBlock();
	nodeFactory.setEndPositionFromNode(block);
	return nodeFactory.createNode<ModifierDefinition>(name, docstring, parameters, isVirtual, overrides, block);
}

ASTPointer<EventDefinition> Parser::parseEventDefinition()
{
	RecursionGuard recursionGuard(*this);
	ASTNodeFactory nodeFactory(*this);
	ASTPointer<ASTString> docstring;
	if (m_scanner->currentCommentLiteral() != "")
		docstring = make_shared<ASTString>(m_scanner->currentCommentLiteral());

	expectToken(Token::Event);
	ASTPointer<ASTString> name(expectIdentifierToken());

	VarDeclParserOptions options;
	options.allowIndexed = true;
	ASTPointer<ParameterList> parameters = parseParameterList(options);

	bool anonymous = false;
	if (m_scanner->currentToken() == Token::Anonymous)
	{
		anonymous = true;
		m_scanner->next();
	}
	nodeFactory.markEndPosition();
	expectToken(Token::Semicolon);
	return nodeFactory.createNode<EventDefinition>(name, docstring, parameters, anonymous);
}

ASTPointer<UsingForDirective> Parser::parseUsingDirective()
{
	RecursionGuard recursionGuard(*this);
	ASTNodeFactory nodeFactory(*this);

	expectToken(Token::Using);
	ASTPointer<UserDefinedTypeName> library(parseUserDefinedTypeName());
	ASTPointer<TypeName> typeName;
	expectToken(Token::For);
	if (m_scanner->currentToken() == Token::Mul)
		m_scanner->next();
	else
		typeName = parseTypeName(false);
	nodeFactory.markEndPosition();
	expectToken(Token::Semicolon);
	return nodeFactory.createNode<UsingForDirective>(library, typeName);
}

ASTPointer<ModifierInvocation> Parser::parseModifierInvocation()
{
	RecursionGuard recursionGuard(*this);
	ASTNodeFactory nodeFactory(*this);
	ASTPointer<Identifier> name(parseIdentifier());
	unique_ptr<vector<ASTPointer<Expression>>> arguments;
	if (m_scanner->currentToken() == Token::LParen)
	{
		m_scanner->next();
		arguments = make_unique<vector<ASTPointer<Expression>>>(parseFunctionCallListArguments());
		nodeFactory.markEndPosition();
		expectToken(Token::RParen);
	}
	else
		nodeFactory.setEndPositionFromNode(name);
	return nodeFactory.createNode<ModifierInvocation>(name, move(arguments));
}

ASTPointer<Identifier> Parser::parseIdentifier()
{
	RecursionGuard recursionGuard(*this);
	ASTNodeFactory nodeFactory(*this);
	nodeFactory.markEndPosition();
	return nodeFactory.createNode<Identifier>(expectIdentifierToken());
}

ASTPointer<UserDefinedTypeName> Parser::parseUserDefinedTypeName()
{
	RecursionGuard recursionGuard(*this);
	ASTNodeFactory nodeFactory(*this);
	nodeFactory.markEndPosition();
	vector<ASTString> identifierPath{*expectIdentifierToken()};
	while (m_scanner->currentToken() == Token::Period)
	{
		m_scanner->next();
		nodeFactory.markEndPosition();
		identifierPath.push_back(*expectIdentifierToken());
	}
	return nodeFactory.createNode<UserDefinedTypeName>(identifierPath);
}

ASTPointer<TypeName> Parser::parseTypeNameSuffix(ASTPointer<TypeName> type, ASTNodeFactory& nodeFactory)
{
	RecursionGuard recursionGuard(*this);
	while (m_scanner->currentToken() == Token::LBrack)
	{
		m_scanner->next();
		ASTPointer<Expression> length;
		if (m_scanner->currentToken() != Token::RBrack)
			length = parseExpression();
		nodeFactory.markEndPosition();
		expectToken(Token::RBrack);
		type = nodeFactory.createNode<ArrayTypeName>(type, length);
	}
	return type;
}

ASTPointer<TypeName> Parser::parseTypeName(bool _allowVar)
{
	RecursionGuard recursionGuard(*this);
	ASTNodeFactory nodeFactory(*this);
	ASTPointer<TypeName> type;
	Token token = m_scanner->currentToken();
	if (TokenTraits::isElementaryTypeName(token))
	{
		unsigned firstSize;
		unsigned secondSize;
		tie(firstSize, secondSize) = m_scanner->currentTokenInfo();
		ElementaryTypeNameToken elemTypeName(token, firstSize, secondSize);
		ASTNodeFactory nodeFactory(*this);
		nodeFactory.markEndPosition();
		m_scanner->next();
		auto stateMutability = elemTypeName.token() == Token::Address
			? optional<StateMutability>{StateMutability::NonPayable}
			: nullopt;
		if (TokenTraits::isStateMutabilitySpecifier(m_scanner->currentToken(), false))
		{
			if (elemTypeName.token() == Token::Address)
			{
				nodeFactory.markEndPosition();
				stateMutability = parseStateMutability();
			}
			else
			{
				parserError("State mutability can only be specified for address types.");
				m_scanner->next();
			}
		}
		type = nodeFactory.createNode<ElementaryTypeName>(elemTypeName, stateMutability);
	}
	else if (token == Token::Var)
	{
		if (!_allowVar)
			parserError(string("Expected explicit type name."));
		m_scanner->next();
	}
	else if (token == Token::Function)
		type = parseFunctionType();
	else if (token == Token::Mapping)
		type = parseMapping();
	else if (token == Token::Identifier)
		type = parseUserDefinedTypeName();
	else
		fatalParserError(string("Expected type name"));

	if (type)
		// Parse "[...]" postfixes for arrays.
		type = parseTypeNameSuffix(type, nodeFactory);
	return type;
}

ASTPointer<FunctionTypeName> Parser::parseFunctionType()
{
	RecursionGuard recursionGuard(*this);
	ASTNodeFactory nodeFactory(*this);
	expectToken(Token::Function);
	FunctionHeaderParserResult header = parseFunctionHeader(true);
	return nodeFactory.createNode<FunctionTypeName>(
		header.parameters,
		header.returnParameters,
		header.visibility,
		header.stateMutability
	);
}

ASTPointer<Mapping> Parser::parseMapping()
{
	RecursionGuard recursionGuard(*this);
	ASTNodeFactory nodeFactory(*this);
	expectToken(Token::Mapping);
	expectToken(Token::LParen);
	ASTPointer<ElementaryTypeName> keyType;
	Token token = m_scanner->currentToken();
	if (!TokenTraits::isElementaryTypeName(token))
		fatalParserError(string("Expected elementary type name for mapping key type"));
	unsigned firstSize;
	unsigned secondSize;
	tie(firstSize, secondSize) = m_scanner->currentTokenInfo();
	ElementaryTypeNameToken elemTypeName(token, firstSize, secondSize);
	keyType = ASTNodeFactory(*this).createNode<ElementaryTypeName>(elemTypeName);
	m_scanner->next();
	expectToken(Token::Arrow);
	bool const allowVar = false;
	ASTPointer<TypeName> valueType = parseTypeName(allowVar);
	nodeFactory.markEndPosition();
	expectToken(Token::RParen);
	return nodeFactory.createNode<Mapping>(keyType, valueType);
}

ASTPointer<ParameterList> Parser::parseParameterList(
	VarDeclParserOptions const& _options,
	bool _allowEmpty
)
{
	RecursionGuard recursionGuard(*this);
	ASTNodeFactory nodeFactory(*this);
	vector<ASTPointer<VariableDeclaration>> parameters;
	VarDeclParserOptions options(_options);
	options.allowEmptyName = true;
	expectToken(Token::LParen);
	if (!_allowEmpty || m_scanner->currentToken() != Token::RParen)
	{
		parameters.push_back(parseVariableDeclaration(options));
		while (m_scanner->currentToken() != Token::RParen)
		{
			if (m_scanner->currentToken() == Token::Comma && m_scanner->peekNextToken() == Token::RParen)
				fatalParserError("Unexpected trailing comma in parameter list.");
			expectToken(Token::Comma);
			parameters.push_back(parseVariableDeclaration(options));
		}
	}
	nodeFactory.markEndPosition();
	m_scanner->next();
	return nodeFactory.createNode<ParameterList>(parameters);
}

ASTPointer<Block> Parser::parseBlock(ASTPointer<ASTString> const& _docString)
{
	RecursionGuard recursionGuard(*this);
	ASTNodeFactory nodeFactory(*this);
	expectToken(Token::LBrace);
	vector<ASTPointer<Statement>> statements;
	try
	{
		while (m_scanner->currentToken() != Token::RBrace)
			statements.push_back(parseStatement());
		nodeFactory.markEndPosition();
	}
	catch (FatalError const&)
	{
		if (
			!m_errorReporter.hasErrors() ||
			!m_parserErrorRecovery ||
			m_errorReporter.hasExcessiveErrors()
		)
			BOOST_THROW_EXCEPTION(FatalError()); /* Don't try to recover here. */
		m_inParserRecovery = true;
	}
	if (m_parserErrorRecovery)
		expectTokenOrConsumeUntil(Token::RBrace, "Block");
	else
		expectToken(Token::RBrace);
	return nodeFactory.createNode<Block>(_docString, statements);
}

ASTPointer<Statement> Parser::parseStatement()
{
	RecursionGuard recursionGuard(*this);
	ASTPointer<ASTString> docString;
	ASTPointer<Statement> statement;
	try
	{
		if (m_scanner->currentCommentLiteral() != "")
			docString = make_shared<ASTString>(m_scanner->currentCommentLiteral());
		switch (m_scanner->currentToken())
		{
		case Token::If:
			return parseIfStatement(docString);
		case Token::While:
			return parseWhileStatement(docString);
		case Token::Do:
			return parseDoWhileStatement(docString);
		case Token::For:
			return parseForStatement(docString);
		case Token::LBrace:
			return parseBlock(docString);
			// starting from here, all statements must be terminated by a semicolon
		case Token::Continue:
			statement = ASTNodeFactory(*this).createNode<Continue>(docString);
			m_scanner->next();
			break;
		case Token::Break:
			statement = ASTNodeFactory(*this).createNode<Break>(docString);
			m_scanner->next();
			break;
		case Token::Return:
		{
			ASTNodeFactory nodeFactory(*this);
			ASTPointer<Expression> expression;
			if (m_scanner->next() != Token::Semicolon)
				{
					expression = parseExpression();
					nodeFactory.setEndPositionFromNode(expression);
				}
			statement = nodeFactory.createNode<Return>(docString, expression);
				break;
		}
		case Token::Throw:
		{
			statement = ASTNodeFactory(*this).createNode<Throw>(docString);
			m_scanner->next();
			break;
		}
		case Token::Try:
			return parseTryStatement(docString);
		case Token::Assembly:
			return parseInlineAssembly(docString);
		case Token::Emit:
			statement = parseEmitStatement(docString);
			break;
		case Token::Identifier:
			if (m_insideModifier && m_scanner->currentLiteral() == "_")
				{
					statement = ASTNodeFactory(*this).createNode<PlaceholderStatement>(docString);
					m_scanner->next();
				}
			else
				statement = parseSimpleStatement(docString);
			break;
		default:
			statement = parseSimpleStatement(docString);
			break;
		}
	}
	catch (FatalError const&)
	{
		if (
			!m_errorReporter.hasErrors() ||
			!m_parserErrorRecovery ||
			m_errorReporter.hasExcessiveErrors()
		)
			BOOST_THROW_EXCEPTION(FatalError()); /* Don't try to recover here. */
		m_inParserRecovery = true;
	}
	if (m_inParserRecovery)
		expectTokenOrConsumeUntil(Token::Semicolon, "Statement");
	else
		expectToken(Token::Semicolon);
	return statement;
}

ASTPointer<InlineAssembly> Parser::parseInlineAssembly(ASTPointer<ASTString> const& _docString)
{
	RecursionGuard recursionGuard(*this);
	SourceLocation location{position(), -1, source()};

	expectToken(Token::Assembly);
	yul::Dialect const& dialect = yul::EVMDialect::strictAssemblyForEVM(m_evmVersion);
	if (m_scanner->currentToken() == Token::StringLiteral)
	{
		if (m_scanner->currentLiteral() != "evmasm")
			fatalParserError("Only \"evmasm\" supported.");
		// This can be used in the future to set the dialect.
		m_scanner->next();
	}

	yul::Parser asmParser(m_errorReporter, dialect);
	shared_ptr<yul::Block> block = asmParser.parse(m_scanner, true);
	if (block == nullptr)
		BOOST_THROW_EXCEPTION(FatalError());

	location.end = block->location.end;
	return make_shared<InlineAssembly>(nextID(), location, _docString, dialect, block);
}

ASTPointer<IfStatement> Parser::parseIfStatement(ASTPointer<ASTString> const& _docString)
{
	RecursionGuard recursionGuard(*this);
	ASTNodeFactory nodeFactory(*this);
	expectToken(Token::If);
	expectToken(Token::LParen);
	ASTPointer<Expression> condition = parseExpression();
	expectToken(Token::RParen);
	ASTPointer<Statement> trueBody = parseStatement();
	ASTPointer<Statement> falseBody;
	if (m_scanner->currentToken() == Token::Else)
	{
		m_scanner->next();
		falseBody = parseStatement();
		nodeFactory.setEndPositionFromNode(falseBody);
	}
	else
		nodeFactory.setEndPositionFromNode(trueBody);
	return nodeFactory.createNode<IfStatement>(_docString, condition, trueBody, falseBody);
}

ASTPointer<TryStatement> Parser::parseTryStatement(ASTPointer<ASTString> const& _docString)
{
	RecursionGuard recursionGuard(*this);
	ASTNodeFactory nodeFactory(*this);
	expectToken(Token::Try);
	ASTPointer<Expression> externalCall = parseExpression();
	vector<ASTPointer<TryCatchClause>> clauses;

	ASTNodeFactory successClauseFactory(*this);
	ASTPointer<ParameterList> returnsParameters;
	if (m_scanner->currentToken() == Token::Returns)
	{
		m_scanner->next();
		VarDeclParserOptions options;
		options.allowEmptyName = true;
		options.allowLocationSpecifier = true;
		returnsParameters = parseParameterList(options, false);
	}
	ASTPointer<Block> successBlock = parseBlock();
	successClauseFactory.setEndPositionFromNode(successBlock);
	clauses.emplace_back(successClauseFactory.createNode<TryCatchClause>(
		make_shared<ASTString>(), returnsParameters, successBlock
	));

	do
	{
		clauses.emplace_back(parseCatchClause());
	}
	while (m_scanner->currentToken() == Token::Catch);
	nodeFactory.setEndPositionFromNode(clauses.back());
	return nodeFactory.createNode<TryStatement>(
		_docString, externalCall, clauses
	);
}

ASTPointer<TryCatchClause> Parser::parseCatchClause()
{
	RecursionGuard recursionGuard(*this);
	ASTNodeFactory nodeFactory(*this);
	expectToken(Token::Catch);
	ASTPointer<ASTString> errorName = make_shared<string>();
	ASTPointer<ParameterList> errorParameters;
	if (m_scanner->currentToken() != Token::LBrace)
	{
		if (m_scanner->currentToken() == Token::Identifier)
			errorName = expectIdentifierToken();
		VarDeclParserOptions options;
		options.allowEmptyName = true;
		options.allowLocationSpecifier = true;
		errorParameters = parseParameterList(options, !errorName->empty());
	}
	ASTPointer<Block> block = parseBlock();
	nodeFactory.setEndPositionFromNode(block);
	return nodeFactory.createNode<TryCatchClause>(errorName, errorParameters, block);
}

ASTPointer<WhileStatement> Parser::parseWhileStatement(ASTPointer<ASTString> const& _docString)
{
	RecursionGuard recursionGuard(*this);
	ASTNodeFactory nodeFactory(*this);
	expectToken(Token::While);
	expectToken(Token::LParen);
	ASTPointer<Expression> condition = parseExpression();
	expectToken(Token::RParen);
	ASTPointer<Statement> body = parseStatement();
	nodeFactory.setEndPositionFromNode(body);
	return nodeFactory.createNode<WhileStatement>(_docString, condition, body, false);
}

ASTPointer<WhileStatement> Parser::parseDoWhileStatement(ASTPointer<ASTString> const& _docString)
{
	RecursionGuard recursionGuard(*this);
	ASTNodeFactory nodeFactory(*this);
	expectToken(Token::Do);
	ASTPointer<Statement> body = parseStatement();
	expectToken(Token::While);
	expectToken(Token::LParen);
	ASTPointer<Expression> condition = parseExpression();
	expectToken(Token::RParen);
	nodeFactory.markEndPosition();
	expectToken(Token::Semicolon);
	return nodeFactory.createNode<WhileStatement>(_docString, condition, body, true);
}


ASTPointer<ForStatement> Parser::parseForStatement(ASTPointer<ASTString> const& _docString)
{
	RecursionGuard recursionGuard(*this);
	ASTNodeFactory nodeFactory(*this);
	ASTPointer<Statement> initExpression;
	ASTPointer<Expression> conditionExpression;
	ASTPointer<ExpressionStatement> loopExpression;
	expectToken(Token::For);
	expectToken(Token::LParen);

	// LTODO: Maybe here have some predicate like peekExpression() instead of checking for semicolon and RParen?
	if (m_scanner->currentToken() != Token::Semicolon)
		initExpression = parseSimpleStatement(ASTPointer<ASTString>());
	expectToken(Token::Semicolon);

	if (m_scanner->currentToken() != Token::Semicolon)
		conditionExpression = parseExpression();
	expectToken(Token::Semicolon);

	if (m_scanner->currentToken() != Token::RParen)
		loopExpression = parseExpressionStatement(ASTPointer<ASTString>());
	expectToken(Token::RParen);

	ASTPointer<Statement> body = parseStatement();
	nodeFactory.setEndPositionFromNode(body);
	return nodeFactory.createNode<ForStatement>(
		_docString,
		initExpression,
		conditionExpression,
		loopExpression,
		body
	);
}

ASTPointer<EmitStatement> Parser::parseEmitStatement(ASTPointer<ASTString> const& _docString)
{
	expectToken(Token::Emit, false);

	ASTNodeFactory nodeFactory(*this);
	m_scanner->next();
	ASTNodeFactory eventCallNodeFactory(*this);

	if (m_scanner->currentToken() != Token::Identifier)
		fatalParserError("Expected event name or path.");

	IndexAccessedPath iap;
	while (true)
	{
		iap.path.push_back(parseIdentifier());
		if (m_scanner->currentToken() != Token::Period)
			break;
		m_scanner->next();
	};

	auto eventName = expressionFromIndexAccessStructure(iap);
	expectToken(Token::LParen);

	vector<ASTPointer<Expression>> arguments;
	vector<ASTPointer<ASTString>> names;
	std::tie(arguments, names) = parseFunctionCallArguments();
	eventCallNodeFactory.markEndPosition();
	nodeFactory.markEndPosition();
	expectToken(Token::RParen);
	auto eventCall = eventCallNodeFactory.createNode<FunctionCall>(eventName, arguments, names);
	auto statement = nodeFactory.createNode<EmitStatement>(_docString, eventCall);
	return statement;
}

ASTPointer<Statement> Parser::parseSimpleStatement(ASTPointer<ASTString> const& _docString)
{
	RecursionGuard recursionGuard(*this);
	LookAheadInfo statementType;
	IndexAccessedPath iap;

	if (m_scanner->currentToken() == Token::LParen)
	{
		ASTNodeFactory nodeFactory(*this);
		size_t emptyComponents = 0;
		// First consume all empty components.
		expectToken(Token::LParen);
		while (m_scanner->currentToken() == Token::Comma)
		{
			m_scanner->next();
			emptyComponents++;
		}

		// Now see whether we have a variable declaration or an expression.
		tie(statementType, iap) = tryParseIndexAccessedPath();
		switch (statementType)
		{
		case LookAheadInfo::VariableDeclaration:
		{
			vector<ASTPointer<VariableDeclaration>> variables;
			ASTPointer<Expression> value;
			// We have already parsed something like `(,,,,a.b.c[2][3]`
			VarDeclParserOptions options;
			options.allowLocationSpecifier = true;
			variables = vector<ASTPointer<VariableDeclaration>>(emptyComponents, nullptr);
			variables.push_back(parseVariableDeclaration(options, typeNameFromIndexAccessStructure(iap)));

			while (m_scanner->currentToken() != Token::RParen)
			{
				expectToken(Token::Comma);
				if (m_scanner->currentToken() == Token::Comma || m_scanner->currentToken() == Token::RParen)
					variables.push_back(nullptr);
				else
					variables.push_back(parseVariableDeclaration(options));
			}
			expectToken(Token::RParen);
			expectToken(Token::Assign);
			value = parseExpression();
			nodeFactory.setEndPositionFromNode(value);
			return nodeFactory.createNode<VariableDeclarationStatement>(_docString, variables, value);
		}
		case LookAheadInfo::Expression:
		{
			// Complete parsing the expression in the current component.
			vector<ASTPointer<Expression>> components(emptyComponents, nullptr);
			components.push_back(parseExpression(expressionFromIndexAccessStructure(iap)));
			while (m_scanner->currentToken() != Token::RParen)
			{
				expectToken(Token::Comma);
				if (m_scanner->currentToken() == Token::Comma || m_scanner->currentToken() == Token::RParen)
					components.push_back(ASTPointer<Expression>());
				else
					components.push_back(parseExpression());
			}
			nodeFactory.markEndPosition();
			expectToken(Token::RParen);
			return parseExpressionStatement(_docString, nodeFactory.createNode<TupleExpression>(components, false));
		}
		default:
			solAssert(false, "");
		}
	}
	else
	{
		tie(statementType, iap) = tryParseIndexAccessedPath();
		switch (statementType)
		{
		case LookAheadInfo::VariableDeclaration:
			return parseVariableDeclarationStatement(_docString, typeNameFromIndexAccessStructure(iap));
		case LookAheadInfo::Expression:
			return parseExpressionStatement(_docString, expressionFromIndexAccessStructure(iap));
		default:
			solAssert(false, "");
		}
	}
}

bool Parser::IndexAccessedPath::empty() const
{
	if (!indices.empty())
	{
		solAssert(!path.empty(), "");
	}
	return path.empty() && indices.empty();
}


pair<Parser::LookAheadInfo, Parser::IndexAccessedPath> Parser::tryParseIndexAccessedPath()
{
	// These two cases are very hard to distinguish:
	// x[7 * 20 + 3] a;     and     x[7 * 20 + 3] = 9;
	// In the first case, x is a type name, in the second it is the name of a variable.
	// As an extension, we can even have:
	// `x.y.z[1][2] a;` and `x.y.z[1][2] = 10;`
	// Where in the first, x.y.z leads to a type name where in the second, it accesses structs.

	auto statementType = peekStatementType();
	switch (statementType)
	{
	case LookAheadInfo::VariableDeclaration:
	case LookAheadInfo::Expression:
		return make_pair(statementType, IndexAccessedPath());
	default:
		break;
	}

	// At this point, we have 'Identifier "["' or 'Identifier "." Identifier' or 'ElementoryTypeName "["'.
	// We parse '(Identifier ("." Identifier)* |ElementaryTypeName) ( "[" Expression "]" )*'
	// until we can decide whether to hand this over to ExpressionStatement or create a
	// VariableDeclarationStatement out of it.
	IndexAccessedPath iap = parseIndexAccessedPath();

	if (m_scanner->currentToken() == Token::Identifier || TokenTraits::isLocationSpecifier(m_scanner->currentToken()))
		return make_pair(LookAheadInfo::VariableDeclaration, move(iap));
	else
		return make_pair(LookAheadInfo::Expression, move(iap));
}

ASTPointer<VariableDeclarationStatement> Parser::parseVariableDeclarationStatement(
	ASTPointer<ASTString> const& _docString,
	ASTPointer<TypeName> const& _lookAheadArrayType
)
{
	// This does not parse multi variable declaration statements starting directly with
	// `(`, they are parsed in parseSimpleStatement, because they are hard to distinguish
	// from tuple expressions.
	RecursionGuard recursionGuard(*this);
	ASTNodeFactory nodeFactory(*this);
	if (_lookAheadArrayType)
		nodeFactory.setLocation(_lookAheadArrayType->location());
	vector<ASTPointer<VariableDeclaration>> variables;
	ASTPointer<Expression> value;
	if (
		!_lookAheadArrayType &&
		m_scanner->currentToken() == Token::Var &&
		m_scanner->peekNextToken() == Token::LParen
	)
	{
		// Parse `var (a, b, ,, c) = ...` into a single VariableDeclarationStatement with multiple variables.
		m_scanner->next();
		m_scanner->next();
		if (m_scanner->currentToken() != Token::RParen)
			while (true)
			{
				ASTPointer<VariableDeclaration> var;
				if (
					m_scanner->currentToken() != Token::Comma &&
					m_scanner->currentToken() != Token::RParen
				)
				{
					ASTNodeFactory varDeclNodeFactory(*this);
					varDeclNodeFactory.markEndPosition();
					ASTPointer<ASTString> name = expectIdentifierToken();
					var = varDeclNodeFactory.createNode<VariableDeclaration>(
						ASTPointer<TypeName>(),
						name,
						ASTPointer<Expression>(),
						Visibility::Default
					);
				}
				variables.push_back(var);
				if (m_scanner->currentToken() == Token::RParen)
					break;
				else
					expectToken(Token::Comma);
			}
		nodeFactory.markEndPosition();
		m_scanner->next();
	}
	else
	{
		VarDeclParserOptions options;
		options.allowVar = true;
		options.allowLocationSpecifier = true;
		variables.push_back(parseVariableDeclaration(options, _lookAheadArrayType));
		nodeFactory.setEndPositionFromNode(variables.back());
	}
	if (m_scanner->currentToken() == Token::Assign)
	{
		m_scanner->next();
		value = parseExpression();
		nodeFactory.setEndPositionFromNode(value);
	}
	return nodeFactory.createNode<VariableDeclarationStatement>(_docString, variables, value);
}

ASTPointer<ExpressionStatement> Parser::parseExpressionStatement(
	ASTPointer<ASTString> const& _docString,
	ASTPointer<Expression> const& _partialParserResult
)
{
	RecursionGuard recursionGuard(*this);
	ASTPointer<Expression> expression = parseExpression(_partialParserResult);
	return ASTNodeFactory(*this, expression).createNode<ExpressionStatement>(_docString, expression);
}

ASTPointer<Expression> Parser::parseExpression(
	ASTPointer<Expression> const& _partiallyParsedExpression
)
{
	RecursionGuard recursionGuard(*this);
	ASTPointer<Expression> expression = parseBinaryExpression(4, _partiallyParsedExpression);
	if (TokenTraits::isAssignmentOp(m_scanner->currentToken()))
	{
		Token assignmentOperator = m_scanner->currentToken();
		m_scanner->next();
		ASTPointer<Expression> rightHandSide = parseExpression();
		ASTNodeFactory nodeFactory(*this, expression);
		nodeFactory.setEndPositionFromNode(rightHandSide);
		return nodeFactory.createNode<Assignment>(expression, assignmentOperator, rightHandSide);
	}
	else if (m_scanner->currentToken() == Token::Conditional)
	{
		m_scanner->next();
		ASTPointer<Expression> trueExpression = parseExpression();
		expectToken(Token::Colon);
		ASTPointer<Expression> falseExpression = parseExpression();
		ASTNodeFactory nodeFactory(*this, expression);
		nodeFactory.setEndPositionFromNode(falseExpression);
		return nodeFactory.createNode<Conditional>(expression, trueExpression, falseExpression);
	}
	else
		return expression;
}

ASTPointer<Expression> Parser::parseBinaryExpression(
	int _minPrecedence,
	ASTPointer<Expression> const& _partiallyParsedExpression
)
{
	RecursionGuard recursionGuard(*this);
	ASTPointer<Expression> expression = parseUnaryExpression(_partiallyParsedExpression);
	ASTNodeFactory nodeFactory(*this, expression);
	int precedence = TokenTraits::precedence(m_scanner->currentToken());
	for (; precedence >= _minPrecedence; --precedence)
		while (TokenTraits::precedence(m_scanner->currentToken()) == precedence)
		{
			Token op = m_scanner->currentToken();
			m_scanner->next();
			ASTPointer<Expression> right = parseBinaryExpression(precedence + 1);
			nodeFactory.setEndPositionFromNode(right);
			expression = nodeFactory.createNode<BinaryOperation>(expression, op, right);
		}
	return expression;
}

ASTPointer<Expression> Parser::parseUnaryExpression(
	ASTPointer<Expression> const& _partiallyParsedExpression
)
{
	RecursionGuard recursionGuard(*this);
	ASTNodeFactory nodeFactory = _partiallyParsedExpression ?
		ASTNodeFactory(*this, _partiallyParsedExpression) : ASTNodeFactory(*this);
	Token token = m_scanner->currentToken();
	if (!_partiallyParsedExpression && (TokenTraits::isUnaryOp(token) || TokenTraits::isCountOp(token)))
	{
		// prefix expression
		m_scanner->next();
		ASTPointer<Expression> subExpression = parseUnaryExpression();
		nodeFactory.setEndPositionFromNode(subExpression);
		return nodeFactory.createNode<UnaryOperation>(token, subExpression, true);
	}
	else
	{
		// potential postfix expression
		ASTPointer<Expression> subExpression = parseLeftHandSideExpression(_partiallyParsedExpression);
		token = m_scanner->currentToken();

		if (!TokenTraits::isCountOp(token))
			return subExpression;
		nodeFactory.markEndPosition();
		m_scanner->next();
		return nodeFactory.createNode<UnaryOperation>(token, subExpression, false);
	}
}

ASTPointer<Expression> Parser::parseLeftHandSideExpression(
	ASTPointer<Expression> const& _partiallyParsedExpression
)
{
	RecursionGuard recursionGuard(*this);
	ASTNodeFactory nodeFactory = _partiallyParsedExpression ?
		ASTNodeFactory(*this, _partiallyParsedExpression) : ASTNodeFactory(*this);

	ASTPointer<Expression> expression;
	if (_partiallyParsedExpression)
		expression = _partiallyParsedExpression;
	else if (m_scanner->currentToken() == Token::New)
	{
		expectToken(Token::New);
		ASTPointer<TypeName> typeName(parseTypeName(false));
		if (typeName)
			nodeFactory.setEndPositionFromNode(typeName);
		else
			nodeFactory.markEndPosition();
		expression = nodeFactory.createNode<NewExpression>(typeName);
	}
	else if (m_scanner->currentToken() == Token::Payable)
	{
		expectToken(Token::Payable);
		nodeFactory.markEndPosition();
		auto expressionType = nodeFactory.createNode<ElementaryTypeName>(
			ElementaryTypeNameToken(Token::Address, 0, 0),
			std::make_optional(StateMutability::Payable)
		);
		expression = nodeFactory.createNode<ElementaryTypeNameExpression>(expressionType);
		expectToken(Token::LParen, false);
	}
	else
		expression = parsePrimaryExpression();

	while (true)
	{
		switch (m_scanner->currentToken())
		{
		case Token::LBrack:
		{
			m_scanner->next();
			ASTPointer<Expression> index;
			ASTPointer<Expression> endIndex;
			if (m_scanner->currentToken() != Token::RBrack && m_scanner->currentToken() != Token::Colon)
				index = parseExpression();
			if (m_scanner->currentToken() == Token::Colon)
			{
				expectToken(Token::Colon);
				if (m_scanner->currentToken() != Token::RBrack)
					endIndex = parseExpression();
				nodeFactory.markEndPosition();
				expectToken(Token::RBrack);
				expression = nodeFactory.createNode<IndexRangeAccess>(expression, index, endIndex);
			}
			else
			{
				nodeFactory.markEndPosition();
				expectToken(Token::RBrack);
				expression = nodeFactory.createNode<IndexAccess>(expression, index);
			}
			break;
		}
		case Token::Period:
		{
			m_scanner->next();
			nodeFactory.markEndPosition();
			if (m_scanner->currentToken() == Token::Address)
			{
				expression = nodeFactory.createNode<MemberAccess>(expression, make_shared<ASTString>("address"));
				m_scanner->next();
			}
			else
				expression = nodeFactory.createNode<MemberAccess>(expression, expectIdentifierToken());
			break;
		}
		case Token::LParen:
		{
			m_scanner->next();
			vector<ASTPointer<Expression>> arguments;
			vector<ASTPointer<ASTString>> names;
			std::tie(arguments, names) = parseFunctionCallArguments();
			nodeFactory.markEndPosition();
			expectToken(Token::RParen);
			expression = nodeFactory.createNode<FunctionCall>(expression, arguments, names);
			break;
		}
		case Token::LBrace:
		{
			// See if this is followed by <identifier>, followed by ":". If not, it is not
			// a function call options but a Block (from a try statement).
			if (
				m_scanner->peekNextToken() != Token::Identifier ||
				m_scanner->peekNextNextToken() != Token::Colon
			)
				return expression;

			expectToken(Token::LBrace);
			auto optionList = parseNamedArguments();

			nodeFactory.markEndPosition();
			expectToken(Token::RBrace);

			expression = parseLeftHandSideExpression(nodeFactory.createNode<FunctionCallOptions>(expression, optionList.first, optionList.second));
			break;
		}
		default:
			return expression;
		}
	}
}

ASTPointer<Expression> Parser::parsePrimaryExpression()
{
	RecursionGuard recursionGuard(*this);
	ASTNodeFactory nodeFactory(*this);
	Token token = m_scanner->currentToken();
	ASTPointer<Expression> expression;

	switch (token)
	{
	case Token::TrueLiteral:
	case Token::FalseLiteral:
		nodeFactory.markEndPosition();
		expression = nodeFactory.createNode<Literal>(token, getLiteralAndAdvance());
		break;
	case Token::Number:
		if (TokenTraits::isEtherSubdenomination(m_scanner->peekNextToken()))
		{
			ASTPointer<ASTString> literal = getLiteralAndAdvance();
			nodeFactory.markEndPosition();
			Literal::SubDenomination subdenomination = static_cast<Literal::SubDenomination>(m_scanner->currentToken());
			m_scanner->next();
			expression = nodeFactory.createNode<Literal>(token, literal, subdenomination);
		}
		else if (TokenTraits::isTimeSubdenomination(m_scanner->peekNextToken()))
		{
			ASTPointer<ASTString> literal = getLiteralAndAdvance();
			nodeFactory.markEndPosition();
			Literal::SubDenomination subdenomination = static_cast<Literal::SubDenomination>(m_scanner->currentToken());
			m_scanner->next();
			expression = nodeFactory.createNode<Literal>(token, literal, subdenomination);
		}
		else
		{
			nodeFactory.markEndPosition();
			expression = nodeFactory.createNode<Literal>(token, getLiteralAndAdvance());
		}
		break;
	case Token::StringLiteral:
	case Token::HexStringLiteral:
	{
		string literal = m_scanner->currentLiteral();
		Token firstToken = m_scanner->currentToken();
		while (m_scanner->peekNextToken() == firstToken)
		{
			m_scanner->next();
			literal += m_scanner->currentLiteral();
		}
		nodeFactory.markEndPosition();
		m_scanner->next();
		if (m_scanner->currentToken() == Token::Illegal)
			fatalParserError(to_string(m_scanner->currentError()));
		expression = nodeFactory.createNode<Literal>(token, make_shared<ASTString>(literal));
		break;
	}
	case Token::Identifier:
		nodeFactory.markEndPosition();
		expression = nodeFactory.createNode<Identifier>(getLiteralAndAdvance());
		break;
	case Token::Type:
		// Inside expressions "type" is the name of a special, globally-available function.
		nodeFactory.markEndPosition();
		m_scanner->next();
		expression = nodeFactory.createNode<Identifier>(make_shared<ASTString>("type"));
		break;
	case Token::LParen:
	case Token::LBrack:
	{
		// Tuple/parenthesized expression or inline array/bracketed expression.
		// Special cases: ()/[] is empty tuple/array type, (x) is not a real tuple,
		// (x,) is one-dimensional tuple, elements in arrays cannot be left out, only in tuples.
		m_scanner->next();
		vector<ASTPointer<Expression>> components;
		Token oppositeToken = (token == Token::LParen ? Token::RParen : Token::RBrack);
		bool isArray = (token == Token::LBrack);

		if (m_scanner->currentToken() != oppositeToken)
			while (true)
			{
				if (m_scanner->currentToken() != Token::Comma && m_scanner->currentToken() != oppositeToken)
					components.push_back(parseExpression());
				else if (isArray)
					parserError("Expected expression (inline array elements cannot be omitted).");
				else
					components.push_back(ASTPointer<Expression>());

				if (m_scanner->currentToken() == oppositeToken)
					break;

				expectToken(Token::Comma);
			}
		nodeFactory.markEndPosition();
		expectToken(oppositeToken);
		expression = nodeFactory.createNode<TupleExpression>(components, isArray);
		break;
	}
	case Token::Illegal:
		fatalParserError(to_string(m_scanner->currentError()));
		break;
	default:
		if (TokenTraits::isElementaryTypeName(token))
		{
			//used for casts
			unsigned firstSize;
			unsigned secondSize;
			tie(firstSize, secondSize) = m_scanner->currentTokenInfo();
			auto expressionType = nodeFactory.createNode<ElementaryTypeName>(
				ElementaryTypeNameToken(m_scanner->currentToken(), firstSize, secondSize)
			);
			expression = nodeFactory.createNode<ElementaryTypeNameExpression>(expressionType);
			m_scanner->next();
		}
		else
			fatalParserError(string("Expected primary expression."));
		break;
	}
	return expression;
}

vector<ASTPointer<Expression>> Parser::parseFunctionCallListArguments()
{
	RecursionGuard recursionGuard(*this);
	vector<ASTPointer<Expression>> arguments;
	if (m_scanner->currentToken() != Token::RParen)
	{
		arguments.push_back(parseExpression());
		while (m_scanner->currentToken() != Token::RParen)
		{
			expectToken(Token::Comma);
			arguments.push_back(parseExpression());
		}
	}
	return arguments;
}

pair<vector<ASTPointer<Expression>>, vector<ASTPointer<ASTString>>> Parser::parseFunctionCallArguments()
{
	RecursionGuard recursionGuard(*this);
	pair<vector<ASTPointer<Expression>>, vector<ASTPointer<ASTString>>> ret;
	Token token = m_scanner->currentToken();
	if (token == Token::LBrace)
	{
		// call({arg1 : 1, arg2 : 2 })
		expectToken(Token::LBrace);
		ret = parseNamedArguments();
		expectToken(Token::RBrace);
	}
	else
		ret.first = parseFunctionCallListArguments();
	return ret;
}

pair<vector<ASTPointer<Expression>>, vector<ASTPointer<ASTString>>> Parser::parseNamedArguments()
{
	pair<vector<ASTPointer<Expression>>, vector<ASTPointer<ASTString>>> ret;

	bool first = true;
	while (m_scanner->currentToken() != Token::RBrace)
	{
		if (!first)
			expectToken(Token::Comma);

		ret.second.push_back(expectIdentifierToken());
		expectToken(Token::Colon);
		ret.first.push_back(parseExpression());

		if (
			m_scanner->currentToken() == Token::Comma &&
			m_scanner->peekNextToken() == Token::RBrace
		)
		{
			parserError("Unexpected trailing comma.");
			m_scanner->next();
		}

		first = false;
	}

	return ret;
}

Parser::LookAheadInfo Parser::peekStatementType() const
{
	// Distinguish between variable declaration (and potentially assignment) and expression statement
	// (which include assignments to other expressions and pre-declared variables).
	// We have a variable declaration if we get a keyword that specifies a type name.
	// If it is an identifier or an elementary type name followed by an identifier
	// or a mutability specifier, we also have a variable declaration.
	// If we get an identifier followed by a "[" or ".", it can be both ("lib.type[9] a;" or "variable.el[9] = 7;").
	// In all other cases, we have an expression statement.
	Token token(m_scanner->currentToken());
	bool mightBeTypeName = (TokenTraits::isElementaryTypeName(token) || token == Token::Identifier);

	if (token == Token::Mapping || token == Token::Function || token == Token::Var)
		return LookAheadInfo::VariableDeclaration;
	if (mightBeTypeName)
	{
		Token next = m_scanner->peekNextToken();
		// So far we only allow ``address payable`` in variable declaration statements and in no other
		// kind of statement. This means, for example, that we do not allow type expressions of the form
		// ``address payable;``.
		// If we want to change this in the future, we need to consider another scanner token here.
		if (TokenTraits::isElementaryTypeName(token) && TokenTraits::isStateMutabilitySpecifier(next, false))
			return LookAheadInfo::VariableDeclaration;
		if (next == Token::Identifier || TokenTraits::isLocationSpecifier(next))
			return LookAheadInfo::VariableDeclaration;
		if (next == Token::LBrack || next == Token::Period)
			return LookAheadInfo::IndexAccessStructure;
	}
	return LookAheadInfo::Expression;
}

Parser::IndexAccessedPath Parser::parseIndexAccessedPath()
{
	IndexAccessedPath iap;
	if (m_scanner->currentToken() == Token::Identifier)
	{
		iap.path.push_back(parseIdentifier());
		while (m_scanner->currentToken() == Token::Period)
		{
			m_scanner->next();
			iap.path.push_back(parseIdentifier());
		}
	}
	else
	{
		unsigned firstNum;
		unsigned secondNum;
		tie(firstNum, secondNum) = m_scanner->currentTokenInfo();
		auto expressionType = ASTNodeFactory(*this).createNode<ElementaryTypeName>(
			ElementaryTypeNameToken(m_scanner->currentToken(), firstNum, secondNum)
		);
		iap.path.push_back(ASTNodeFactory(*this).createNode<ElementaryTypeNameExpression>(expressionType));
		m_scanner->next();
	}
	while (m_scanner->currentToken() == Token::LBrack)
	{
		expectToken(Token::LBrack);
		ASTPointer<Expression> index;
		if (m_scanner->currentToken() != Token::RBrack && m_scanner->currentToken() != Token::Colon)
			index = parseExpression();
		SourceLocation indexLocation = iap.path.front()->location();
		if (m_scanner->currentToken() == Token::Colon)
		{
			expectToken(Token::Colon);
			ASTPointer<Expression> endIndex;
			if (m_scanner->currentToken() != Token::RBrack)
				endIndex = parseExpression();
			indexLocation.end = endPosition();
			iap.indices.emplace_back(IndexAccessedPath::Index{index, {endIndex}, indexLocation});
			expectToken(Token::RBrack);
		}
		else
		{
			indexLocation.end = endPosition();
			iap.indices.emplace_back(IndexAccessedPath::Index{index, {}, indexLocation});
			expectToken(Token::RBrack);
		}
	}

	return iap;
}

ASTPointer<TypeName> Parser::typeNameFromIndexAccessStructure(Parser::IndexAccessedPath const& _iap)
{
	if (_iap.empty())
		return {};

	RecursionGuard recursionGuard(*this);
	ASTNodeFactory nodeFactory(*this);
	SourceLocation location = _iap.path.front()->location();
	location.end = _iap.path.back()->location().end;
	nodeFactory.setLocation(location);

	ASTPointer<TypeName> type;
	if (auto typeName = dynamic_cast<ElementaryTypeNameExpression const*>(_iap.path.front().get()))
	{
		solAssert(_iap.path.size() == 1, "");
		type = nodeFactory.createNode<ElementaryTypeName>(typeName->type().typeName());
	}
	else
	{
		vector<ASTString> path;
		for (auto const& el: _iap.path)
			path.push_back(dynamic_cast<Identifier const&>(*el).name());
		type = nodeFactory.createNode<UserDefinedTypeName>(path);
	}
	for (auto const& lengthExpression: _iap.indices)
	{
		if (lengthExpression.end)
			parserError(lengthExpression.location, "Expected array length expression.");
		nodeFactory.setLocation(lengthExpression.location);
		type = nodeFactory.createNode<ArrayTypeName>(type, lengthExpression.start);
	}
	return type;
}

ASTPointer<Expression> Parser::expressionFromIndexAccessStructure(
	Parser::IndexAccessedPath const& _iap
)
{
	if (_iap.empty())
		return {};

	RecursionGuard recursionGuard(*this);
	ASTNodeFactory nodeFactory(*this, _iap.path.front());
	ASTPointer<Expression> expression(_iap.path.front());
	for (size_t i = 1; i < _iap.path.size(); ++i)
	{
		SourceLocation location(_iap.path.front()->location());
		location.end = _iap.path[i]->location().end;
		nodeFactory.setLocation(location);
		Identifier const& identifier = dynamic_cast<Identifier const&>(*_iap.path[i]);
		expression = nodeFactory.createNode<MemberAccess>(
			expression,
			make_shared<ASTString>(identifier.name())
		);
	}
	for (auto const& index: _iap.indices)
	{
		nodeFactory.setLocation(index.location);
		if (index.end)
			expression = nodeFactory.createNode<IndexRangeAccess>(expression, index.start, *index.end);
		else
			expression = nodeFactory.createNode<IndexAccess>(expression, index.start);
	}
	return expression;
}

ASTPointer<ParameterList> Parser::createEmptyParameterList()
{
	RecursionGuard recursionGuard(*this);
	ASTNodeFactory nodeFactory(*this);
	nodeFactory.setLocationEmpty();
	return nodeFactory.createNode<ParameterList>(vector<ASTPointer<VariableDeclaration>>());
}

ASTPointer<ASTString> Parser::expectIdentifierToken()
{
	// do not advance on success
	expectToken(Token::Identifier, false);
	return getLiteralAndAdvance();
}

ASTPointer<ASTString> Parser::getLiteralAndAdvance()
{
	ASTPointer<ASTString> identifier = make_shared<ASTString>(m_scanner->currentLiteral());
	m_scanner->next();
	return identifier;
}

}
