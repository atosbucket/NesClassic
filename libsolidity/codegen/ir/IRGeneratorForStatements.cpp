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
 * Component that translates Solidity code into Yul at statement level and below.
 */

#include <libsolidity/codegen/ir/IRGeneratorForStatements.h>

#include <libsolidity/codegen/ir/IRGenerationContext.h>
#include <libsolidity/codegen/YulUtilFunctions.h>

#include <libyul/AsmPrinter.h>
#include <libyul/AsmData.h>
#include <libyul/optimiser/ASTCopier.h>

#include <libdevcore/StringUtils.h>

using namespace std;
using namespace dev;
using namespace dev::solidity;

namespace
{

struct CopyTranslate: public yul::ASTCopier
{
	using ExternalRefsMap = std::map<yul::Identifier const*, InlineAssemblyAnnotation::ExternalIdentifierInfo>;

	CopyTranslate(IRGenerationContext& _context, ExternalRefsMap const& _references):
		m_context(_context), m_references(_references) {}

	using ASTCopier::operator();

	yul::YulString translateIdentifier(yul::YulString _name) override
	{
		return yul::YulString{"usr$" + _name.str()};
	}

	yul::Identifier translate(yul::Identifier const& _identifier) override
	{
		if (!m_references.count(&_identifier))
			return ASTCopier::translate(_identifier);

		auto const& reference = m_references.at(&_identifier);
		auto const varDecl = dynamic_cast<VariableDeclaration const*>(reference.declaration);
		solUnimplementedAssert(varDecl, "");
		solUnimplementedAssert(
			reference.isOffset == false && reference.isSlot == false,
			""
		);

		return yul::Identifier{
			_identifier.location,
			yul::YulString{m_context.variableName(*varDecl)}
		};
	}

private:
	IRGenerationContext& m_context;
	ExternalRefsMap const& m_references;
};

}



bool IRGeneratorForStatements::visit(VariableDeclarationStatement const& _varDeclStatement)
{
	for (auto const& decl: _varDeclStatement.declarations())
		if (decl)
			m_context.addLocalVariable(*decl);

	if (Expression const* expression = _varDeclStatement.initialValue())
	{
		solUnimplementedAssert(_varDeclStatement.declarations().size() == 1, "");

		expression->accept(*this);

		solUnimplementedAssert(
			*expression->annotation().type == *_varDeclStatement.declarations().front()->type(),
			"Type conversion not yet implemented"
		);
		m_code <<
			"let " <<
			m_context.variableName(*_varDeclStatement.declarations().front()) <<
			" := " <<
			m_context.variable(*expression) <<
			"\n";
	}
	else
		for (auto const& decl: _varDeclStatement.declarations())
			if (decl)
				m_code << "let " << m_context.variableName(*decl) << "\n";

	return false;
}

bool IRGeneratorForStatements::visit(Assignment const& _assignment)
{
	solUnimplementedAssert(_assignment.assignmentOperator() == Token::Assign, "");

	_assignment.rightHandSide().accept(*this);

//	solUnimplementedAssert(
//		*_assignment.rightHandSide().annotation().type == *_assignment.leftHandSide().annotation().type,
//		"Type conversion not yet implemented"
//	);
	// TODO proper lvalue handling
	auto const& identifier = dynamic_cast<Identifier const&>(_assignment.leftHandSide());
	string varName = m_context.variableName(dynamic_cast<VariableDeclaration const&>(*identifier.annotation().referencedDeclaration));
	m_code << varName << " := " << m_context.variable(_assignment.rightHandSide()) << "\n";
	m_code << "let " << m_context.variable(_assignment) << " := " << varName << "\n";
	return false;
}

void IRGeneratorForStatements::endVisit(BinaryOperation const& _binOp)
{
	solUnimplementedAssert(_binOp.getOperator() == Token::Add, "");
	solUnimplementedAssert(*_binOp.leftExpression().annotation().type == *_binOp.rightExpression().annotation().type, "");
	if (IntegerType const* type = dynamic_cast<IntegerType const*>(_binOp.annotation().commonType))
	{
		solUnimplementedAssert(!type->isSigned(), "");
		m_code <<
			"let " <<
			m_context.variable(_binOp) <<
			" := " <<
			m_utils.overflowCheckedUIntAddFunction(type->numBits()) <<
			"(" <<
			m_context.variable(_binOp.leftExpression()) <<
			", " <<
			m_context.variable(_binOp.rightExpression()) <<
			")\n";
	}
	else
		solUnimplementedAssert(false, "");
}

bool IRGeneratorForStatements::visit(FunctionCall const& _functionCall)
{
	solUnimplementedAssert(_functionCall.annotation().kind == FunctionCallKind::FunctionCall, "");
	FunctionTypePointer functionType = dynamic_cast<FunctionType const*>(_functionCall.expression().annotation().type);

	TypePointers parameterTypes = functionType->parameterTypes();
	vector<ASTPointer<Expression const>> const& callArguments = _functionCall.arguments();
	vector<ASTPointer<ASTString>> const& callArgumentNames = _functionCall.names();
	if (!functionType->takesArbitraryParameters())
		solAssert(callArguments.size() == parameterTypes.size(), "");

	vector<ASTPointer<Expression const>> arguments;
	if (callArgumentNames.empty())
		// normal arguments
		arguments = callArguments;
	else
		// named arguments
		for (auto const& parameterName: functionType->parameterNames())
		{
			auto const it = std::find_if(callArgumentNames.cbegin(), callArgumentNames.cend(), [&](ASTPointer<ASTString> const& _argName) {
				return *_argName == parameterName;
			});

			solAssert(it != callArgumentNames.cend(), "");
			arguments.push_back(callArguments[std::distance(callArgumentNames.begin(), it)]);
		}

	solUnimplementedAssert(!functionType->bound(), "");
	switch (functionType->kind())
	{
	case FunctionType::Kind::Internal:
	{
		vector<string> args;
		for (unsigned i = 0; i < arguments.size(); ++i)
		{
			arguments[i]->accept(*this);
			// TODO convert
			//utils().convertType(*arguments[i]->annotation().type, *function.parameterTypes()[i]);
			args.emplace_back(m_context.variable(*arguments[i]));
		}

		if (auto identifier = dynamic_cast<Identifier const*>(&_functionCall.expression()))
		{
			solAssert(!functionType->bound(), "");
			if (auto functionDef = dynamic_cast<FunctionDefinition const*>(identifier->annotation().referencedDeclaration))
			{
				// @TODO The function can very well return multiple vars.
				m_code <<
					"let " <<
					m_context.variable(_functionCall) <<
					" := " <<
					m_context.virtualFunctionName(*functionDef) <<
					"(" <<
					joinHumanReadable(args) <<
					")\n";
				return false;
			}
		}

		_functionCall.expression().accept(*this);

		// @TODO The function can very well return multiple vars.
		args = vector<string>{m_context.variable(_functionCall.expression())} + args;
		m_code <<
			"let " <<
			m_context.variable(_functionCall) <<
			" := " <<
			m_context.internalDispatch(functionType->parameterTypes().size(), functionType->returnParameterTypes().size()) <<
			"(" <<
			joinHumanReadable(args) <<
			")\n";
		break;
	}
	default:
		solUnimplemented("");
	}
	return false;
}

bool IRGeneratorForStatements::visit(InlineAssembly const& _inlineAsm)
{
	CopyTranslate bodyCopier{m_context, _inlineAsm.annotation().externalReferences};

	yul::Statement modified = bodyCopier(_inlineAsm.operations());

	solAssert(modified.type() == typeid(yul::Block), "");

	m_code << yul::AsmPrinter()(boost::get<yul::Block>(std::move(modified))) << "\n";
	return false;
}

bool IRGeneratorForStatements::visit(Identifier const& _identifier)
{
	Declaration const* declaration = _identifier.annotation().referencedDeclaration;
	string value;
	if (FunctionDefinition const* functionDef = dynamic_cast<FunctionDefinition const*>(declaration))
		value = to_string(m_context.virtualFunction(*functionDef).id());
	else if (VariableDeclaration const* varDecl = dynamic_cast<VariableDeclaration const*>(declaration))
		value = m_context.variableName(*varDecl);
	else
		solUnimplemented("");
	m_code << "let " << m_context.variable(_identifier) << " := " << value << "\n";
	return false;
}

bool IRGeneratorForStatements::visit(Literal const& _literal)
{
	TypePointer type = _literal.annotation().type;

	switch (type->category())
	{
	case Type::Category::RationalNumber:
	case Type::Category::Bool:
	case Type::Category::Address:
		m_code << "let " << m_context.variable(_literal) << " := " << toCompactHexWithPrefix(type->literalValue(&_literal)) << "\n";
		break;
	case Type::Category::StringLiteral:
		solUnimplemented("");
		break; // will be done during conversion
	default:
		solUnimplemented("Only integer, boolean and string literals implemented for now.");
	}
	return false;
}
