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
 * Yul dialects for EVM.
 */

#include <libyul/backends/evm/EVMDialect.h>

#include <libyul/AsmAnalysisInfo.h>
#include <libyul/AsmData.h>
#include <libyul/Object.h>
#include <libyul/Exceptions.h>
#include <libyul/AsmParser.h>
#include <libyul/backends/evm/AbstractAssembly.h>

#include <libevmasm/SemanticInformation.h>
#include <libevmasm/Instruction.h>

#include <liblangutil/Exceptions.h>

#include <boost/range/adaptor/reversed.hpp>

using namespace std;
using namespace solidity;
using namespace solidity::yul;
using namespace solidity::util;

namespace
{
pair<YulString, BuiltinFunctionForEVM> createEVMFunction(
	string const& _name,
	evmasm::Instruction _instruction
)
{
	evmasm::InstructionInfo info = evmasm::instructionInfo(_instruction);
	BuiltinFunctionForEVM f;
	f.name = YulString{_name};
	f.parameters.resize(info.args);
	f.returns.resize(info.ret);
	f.sideEffects = EVMDialect::sideEffectsOfInstruction(_instruction);
	f.isMSize = _instruction == evmasm::Instruction::MSIZE;
	f.literalArguments = false;
	f.instruction = _instruction;
	f.generateCode = [_instruction](
		FunctionCall const&,
		AbstractAssembly& _assembly,
		BuiltinContext&,
		std::function<void()> _visitArguments
	) {
		_visitArguments();
		_assembly.appendInstruction(_instruction);
	};

	return {f.name, move(f)};
}

pair<YulString, BuiltinFunctionForEVM> createFunction(
	string _name,
	size_t _params,
	size_t _returns,
	SideEffects _sideEffects,
	bool _literalArguments,
	std::function<void(FunctionCall const&, AbstractAssembly&, BuiltinContext&, std::function<void()>)> _generateCode
)
{
	YulString name{std::move(_name)};
	BuiltinFunctionForEVM f;
	f.name = name;
	f.parameters.resize(_params);
	f.returns.resize(_returns);
	f.sideEffects = std::move(_sideEffects);
	f.literalArguments = _literalArguments;
	f.isMSize = false;
	f.instruction = {};
	f.generateCode = std::move(_generateCode);
	return {name, f};
}

map<YulString, BuiltinFunctionForEVM> createBuiltins(langutil::EVMVersion _evmVersion, bool _objectAccess)
{
	map<YulString, BuiltinFunctionForEVM> builtins;
	for (auto const& instr: Parser::instructions())
		if (
			!evmasm::isDupInstruction(instr.second) &&
			!evmasm::isSwapInstruction(instr.second) &&
			instr.second != evmasm::Instruction::JUMP &&
			instr.second != evmasm::Instruction::JUMPI &&
			_evmVersion.hasOpcode(instr.second)
		)
			builtins.emplace(createEVMFunction(instr.first, instr.second));

	if (_objectAccess)
	{
		builtins.emplace(createFunction("datasize", 1, 1, SideEffects{}, true, [](
			FunctionCall const& _call,
			AbstractAssembly& _assembly,
			BuiltinContext& _context,
			function<void()>
		) {
			yulAssert(_context.currentObject, "No object available.");
			yulAssert(_call.arguments.size() == 1, "");
			Expression const& arg = _call.arguments.front();
			YulString dataName = std::get<Literal>(arg).value;
			if (_context.currentObject->name == dataName)
				_assembly.appendAssemblySize();
			else
			{
				yulAssert(
					_context.subIDs.count(dataName) != 0,
					"Could not find assembly object <" + dataName.str() + ">."
				);
				_assembly.appendDataSize(_context.subIDs.at(dataName));
			}
		}));
		builtins.emplace(createFunction("dataoffset", 1, 1, SideEffects{}, true, [](
			FunctionCall const& _call,
			AbstractAssembly& _assembly,
			BuiltinContext& _context,
			std::function<void()>
		) {
			yulAssert(_context.currentObject, "No object available.");
			yulAssert(_call.arguments.size() == 1, "");
			Expression const& arg = _call.arguments.front();
			YulString dataName = std::get<Literal>(arg).value;
			if (_context.currentObject->name == dataName)
				_assembly.appendConstant(0);
			else
			{
				yulAssert(
					_context.subIDs.count(dataName) != 0,
					"Could not find assembly object <" + dataName.str() + ">."
				);
				_assembly.appendDataOffset(_context.subIDs.at(dataName));
			}
		}));
		builtins.emplace(createFunction(
			"datacopy",
			3,
			0,
			SideEffects{false, false, false, false, true},
			false,
			[](
				FunctionCall const&,
				AbstractAssembly& _assembly,
				BuiltinContext&,
				std::function<void()> _visitArguments
			) {
				_visitArguments();
				_assembly.appendInstruction(evmasm::Instruction::CODECOPY);
			}
		));
	}
	return builtins;
}

}

EVMDialect::EVMDialect(langutil::EVMVersion _evmVersion, bool _objectAccess):
	m_objectAccess(_objectAccess),
	m_evmVersion(_evmVersion),
	m_functions(createBuiltins(_evmVersion, _objectAccess))
{
}

BuiltinFunctionForEVM const* EVMDialect::builtin(YulString _name) const
{
	auto it = m_functions.find(_name);
	if (it != m_functions.end())
		return &it->second;
	else
		return nullptr;
}

EVMDialect const& EVMDialect::strictAssemblyForEVM(langutil::EVMVersion _version)
{
	static map<langutil::EVMVersion, unique_ptr<EVMDialect const>> dialects;
	static YulStringRepository::ResetCallback callback{[&] { dialects.clear(); }};
	if (!dialects[_version])
		dialects[_version] = make_unique<EVMDialect>(_version, false);
	return *dialects[_version];
}

EVMDialect const& EVMDialect::strictAssemblyForEVMObjects(langutil::EVMVersion _version)
{
	static map<langutil::EVMVersion, unique_ptr<EVMDialect const>> dialects;
	static YulStringRepository::ResetCallback callback{[&] { dialects.clear(); }};
	if (!dialects[_version])
		dialects[_version] = make_unique<EVMDialect>(_version, true);
	return *dialects[_version];
}

SideEffects EVMDialect::sideEffectsOfInstruction(evmasm::Instruction _instruction)
{
	return SideEffects{
		evmasm::SemanticInformation::movable(_instruction),
		evmasm::SemanticInformation::sideEffectFree(_instruction),
		evmasm::SemanticInformation::sideEffectFreeIfNoMSize(_instruction),
		evmasm::SemanticInformation::invalidatesStorage(_instruction),
		evmasm::SemanticInformation::invalidatesMemory(_instruction)
	};
}

EVMDialectTyped::EVMDialectTyped(langutil::EVMVersion _evmVersion, bool _objectAccess):
	EVMDialect(_evmVersion, _objectAccess)
{
	defaultType = "u256"_yulstring;
	boolType = "bool"_yulstring;
	types = {defaultType, boolType};

	// Set all types to ``defaultType``
	for (auto& fun: m_functions)
	{
		for (auto& p: fun.second.parameters)
			p = defaultType;
		for (auto& r: fun.second.returns)
			r = defaultType;
	}

	m_functions["lt"_yulstring].returns = {"bool"_yulstring};
	m_functions["gt"_yulstring].returns = {"bool"_yulstring};
	m_functions["slt"_yulstring].returns = {"bool"_yulstring};
	m_functions["sgt"_yulstring].returns = {"bool"_yulstring};
	m_functions["eq"_yulstring].returns = {"bool"_yulstring};

	// "not" and "bitnot" replace "iszero" and "not"
	m_functions["bitnot"_yulstring] = m_functions["not"_yulstring];
	m_functions["bitnot"_yulstring].name = "bitnot"_yulstring;
	m_functions["not"_yulstring] = m_functions["iszero"_yulstring];
	m_functions["not"_yulstring].name = "not"_yulstring;
	m_functions["not"_yulstring].returns = {"bool"_yulstring};
	m_functions["not"_yulstring].parameters = {"bool"_yulstring};
	m_functions.erase("iszero"_yulstring);

	m_functions["bitand"_yulstring] = m_functions["and"_yulstring];
	m_functions["bitand"_yulstring].name = "bitand"_yulstring;
	m_functions["bitor"_yulstring] = m_functions["or"_yulstring];
	m_functions["bitor"_yulstring].name = "bitor"_yulstring;
	m_functions["bitxor"_yulstring] = m_functions["xor"_yulstring];
	m_functions["bitxor"_yulstring].name = "bitxor"_yulstring;
	m_functions["and"_yulstring].parameters = {"bool"_yulstring, "bool"_yulstring};
	m_functions["and"_yulstring].returns = {"bool"_yulstring};
	m_functions["or"_yulstring].parameters = {"bool"_yulstring, "bool"_yulstring};
	m_functions["or"_yulstring].returns = {"bool"_yulstring};
	m_functions["xor"_yulstring].parameters = {"bool"_yulstring, "bool"_yulstring};
	m_functions["xor"_yulstring].returns = {"bool"_yulstring};
	m_functions["popbool"_yulstring] = m_functions["pop"_yulstring];
	m_functions["popbool"_yulstring].name = "popbool"_yulstring;
	m_functions["popbool"_yulstring].parameters = {"bool"_yulstring};
	m_functions.insert(createFunction("bool_to_u256", 1, 1, {}, false, [](
		FunctionCall const&,
		AbstractAssembly&,
		BuiltinContext&,
		std::function<void()> _visitArguments
	) {
		_visitArguments();
	}));
	m_functions["bool_to_u256"_yulstring].parameters = {"bool"_yulstring};
	m_functions["bool_to_u256"_yulstring].returns = {"u256"_yulstring};
	m_functions.insert(createFunction("u256_to_bool", 1, 1, {}, false, [](
		FunctionCall const&,
		AbstractAssembly& _assembly,
		BuiltinContext&,
		std::function<void()> _visitArguments
	) {
		// A value larger than 1 causes an invalid instruction.
		_visitArguments();
		_assembly.appendConstant(2);
		_assembly.appendInstruction(evmasm::Instruction::DUP2);
		_assembly.appendInstruction(evmasm::Instruction::LT);
		AbstractAssembly::LabelID inRange = _assembly.newLabelId();
		_assembly.appendJumpToIf(inRange);
		_assembly.appendInstruction(evmasm::Instruction::INVALID);
		_assembly.appendLabel(inRange);
	}));
	m_functions["u256_to_bool"_yulstring].parameters = {"u256"_yulstring};
	m_functions["u256_to_bool"_yulstring].returns = {"bool"_yulstring};
}

BuiltinFunctionForEVM const* EVMDialectTyped::discardFunction(YulString _type) const
{
	if (_type == "bool"_yulstring)
		return builtin("popbool"_yulstring);
	else
	{
		yulAssert(_type == defaultType, "");
		return builtin("pop"_yulstring);
	}
}

BuiltinFunctionForEVM const* EVMDialectTyped::equalityFunction(YulString _type) const
{
	if (_type == "bool"_yulstring)
		return nullptr;
	else
	{
		yulAssert(_type == defaultType, "");
		return builtin("eq"_yulstring);
	}
}

EVMDialectTyped const& EVMDialectTyped::instance(langutil::EVMVersion _version)
{
	static map<langutil::EVMVersion, unique_ptr<EVMDialectTyped const>> dialects;
	static YulStringRepository::ResetCallback callback{[&] { dialects.clear(); }};
	if (!dialects[_version])
		dialects[_version] = make_unique<EVMDialectTyped>(_version, true);
	return *dialects[_version];
}
