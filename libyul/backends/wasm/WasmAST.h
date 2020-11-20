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
 * Simplified in-memory representation of a Wasm AST.
 */

#pragma once

#include <libsolutil/Common.h>

#include <variant>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>

namespace solidity::yul::wasm
{

enum class Type
{
	i32,
	i64,
};

struct TypedName { std::string name; Type type; };
using TypedNameList = std::vector<TypedName>;

struct Literal;
struct StringLiteral;
struct LocalVariable;
struct GlobalVariable;
struct FunctionCall;
struct BuiltinCall;
struct LocalAssignment;
struct GlobalAssignment;
struct Block;
struct If;
struct Loop;
struct Branch;
struct BranchIf;
struct Return;
using Expression = std::variant<
	Literal, StringLiteral, LocalVariable, GlobalVariable,
	FunctionCall, BuiltinCall, LocalAssignment, GlobalAssignment,
	Block, If, Loop, Branch, BranchIf, Return
>;

struct Literal { std::variant<uint32_t, uint64_t> value; };
// This is a special AST element used for certain builtins. It is not mapped to actual WebAssembly.
struct StringLiteral { std::string value; };
struct LocalVariable { std::string name; };
struct GlobalVariable { std::string name; };
struct Label { std::string name; };
struct FunctionCall { std::string functionName; std::vector<Expression> arguments; };
struct BuiltinCall { std::string functionName; std::vector<Expression> arguments; };
struct LocalAssignment { std::string variableName; std::unique_ptr<Expression> value; };
struct GlobalAssignment { std::string variableName; std::unique_ptr<Expression> value; };
struct Block { std::string labelName; std::vector<Expression> statements; };
struct If {
	std::unique_ptr<Expression> condition;
	std::vector<Expression> statements;
	std::unique_ptr<std::vector<Expression>> elseStatements;
};
struct Loop { std::string labelName; std::vector<Expression> statements; };
struct Branch { Label label; };
struct Return {};
struct BranchIf { Label label; std::unique_ptr<Expression> condition; };

struct VariableDeclaration { std::string variableName; Type type; };
struct GlobalVariableDeclaration { std::string variableName; Type type; };
struct FunctionImport {
	std::string module;
	std::string externalName;
	std::string internalName;
	std::vector<Type> paramTypes;
	std::optional<Type> returnType;
};

struct FunctionDefinition
{
	std::string name;
	std::vector<TypedName> parameters;
	std::optional<Type> returnType;
	std::vector<VariableDeclaration> locals;
	std::vector<Expression> body;
};

/**
 * Abstract representation of a wasm module.
 */
struct Module
{
	std::vector<GlobalVariableDeclaration> globals;
	std::vector<FunctionImport> imports;
	std::vector<FunctionDefinition> functions;
	std::map<std::string, Module> subModules;
	std::map<std::string, bytes> customSections;
};

}
