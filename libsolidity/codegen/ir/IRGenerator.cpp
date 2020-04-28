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
 * @author Alex Beregszaszi
 * @date 2017
 * Component that translates Solidity code into Yul.
 */

#include <libsolidity/codegen/ir/IRGenerator.h>

#include <libsolidity/codegen/ir/IRGeneratorForStatements.h>

#include <libsolidity/ast/AST.h>
#include <libsolidity/ast/ASTVisitor.h>
#include <libsolidity/codegen/ABIFunctions.h>
#include <libsolidity/codegen/CompilerUtils.h>

#include <libyul/AssemblyStack.h>
#include <libyul/Utilities.h>

#include <libsolutil/CommonData.h>
#include <libsolutil/Whiskers.h>
#include <libsolutil/StringUtils.h>

#include <liblangutil/SourceReferenceFormatter.h>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/range/adaptor/reversed.hpp>

#include <sstream>

using namespace std;
using namespace solidity;
using namespace solidity::util;
using namespace solidity::frontend;

pair<string, string> IRGenerator::run(
	ContractDefinition const& _contract,
	map<ContractDefinition const*, string const> const& _otherYulSources
)
{
	string const ir = yul::reindent(generate(_contract, _otherYulSources));

	yul::AssemblyStack asmStack(m_evmVersion, yul::AssemblyStack::Language::StrictAssembly, m_optimiserSettings);
	if (!asmStack.parseAndAnalyze("", ir))
	{
		string errorMessage;
		for (auto const& error: asmStack.errors())
			errorMessage += langutil::SourceReferenceFormatter::formatErrorInformation(*error);
		solAssert(false, ir + "\n\nInvalid IR generated:\n" + errorMessage + "\n");
	}
	asmStack.optimize();

	string warning =
		"/*******************************************************\n"
		" *                       WARNING                       *\n"
		" *  Solidity to Yul compilation is still EXPERIMENTAL  *\n"
		" *       It can result in LOSS OF FUNDS or worse       *\n"
		" *                !USE AT YOUR OWN RISK!               *\n"
		" *******************************************************/\n\n";

	return {warning + ir, warning + asmStack.print()};
}

string IRGenerator::generate(
	ContractDefinition const& _contract,
	map<ContractDefinition const*, string const> const& _otherYulSources
)
{
	auto subObjectSources = [&_otherYulSources](std::set<ContractDefinition const*, ASTNode::CompareByID> const& subObjects) -> string
	{
		std::string subObjectsSources;
		for (ContractDefinition const* subObject: subObjects)
			subObjectsSources += _otherYulSources.at(subObject);
		return subObjectsSources;
	};

	Whiskers t(R"(
		object "<CreationObject>" {
			code {
				<memoryInit>
				<callValueCheck>
				<?notLibrary>
				<?constructorHasParams> let <constructorParams> := <copyConstructorArguments>() </constructorHasParams>
				<implicitConstructor>(<constructorParams>)
				</notLibrary>
				<deploy>
				<functions>
			}
			object "<RuntimeObject>" {
				code {
					<memoryInit>
					<dispatch>
					<runtimeFunctions>
				}
				<runtimeSubObjects>
			}
			<subObjects>
		}
	)");

	resetContext(_contract);

	t("CreationObject", m_context.creationObjectName(_contract));
	t("memoryInit", memoryInit());
	t("notLibrary", !_contract.isLibrary());

	FunctionDefinition const* constructor = _contract.constructor();
	t("callValueCheck", !constructor || !constructor->isPayable() ? callValueCheck() : "");
	vector<string> constructorParams;
	if (constructor && !constructor->parameters().empty())
	{
		for (size_t i = 0; i < constructor->parameters().size(); ++i)
			constructorParams.emplace_back(m_context.newYulVariable());
		t(
			"copyConstructorArguments",
			m_utils.copyConstructorArgumentsToMemoryFunction(_contract, m_context.creationObjectName(_contract))
		);
	}
	t("constructorParams", joinHumanReadable(constructorParams));
	t("constructorHasParams", !constructorParams.empty());
	t("implicitConstructor", implicitConstructorName(_contract));

	t("deploy", deployCode(_contract));
	generateImplicitConstructors(_contract);
	generateQueuedFunctions();
	t("functions", m_context.functionCollector().requestedFunctions());
	t("subObjects", subObjectSources(m_context.subObjectsCreated()));

	resetContext(_contract);
	t("RuntimeObject", m_context.runtimeObjectName(_contract));
	t("dispatch", dispatchRoutine(_contract));
	generateQueuedFunctions();
	t("runtimeFunctions", m_context.functionCollector().requestedFunctions());
	t("runtimeSubObjects", subObjectSources(m_context.subObjectsCreated()));
	return t.render();
}

string IRGenerator::generate(Block const& _block)
{
	IRGeneratorForStatements generator(m_context, m_utils);
	_block.accept(generator);
	return generator.code();
}

void IRGenerator::generateQueuedFunctions()
{
	while (!m_context.functionGenerationQueueEmpty())
		// NOTE: generateFunction() may modify function generation queue
		generateFunction(*m_context.dequeueFunctionForCodeGeneration());
}

string IRGenerator::generateFunction(FunctionDefinition const& _function)
{
	string functionName = m_context.functionName(_function);
	return m_context.functionCollector().createFunction(functionName, [&]() {
		Whiskers t(R"(
			function <functionName>(<params>) <returns> {
				<initReturnVariables>
				<body>
			}
		)");
		t("functionName", functionName);
		string params;
		for (auto const& varDecl: _function.parameters())
			params += (params.empty() ? "" : ", ") + m_context.addLocalVariable(*varDecl).commaSeparatedList();
		t("params", params);
		string retParams;
		string retInit;
		for (auto const& varDecl: _function.returnParameters())
		{
			retParams += (retParams.empty() ? "" : ", ") + m_context.addLocalVariable(*varDecl).commaSeparatedList();
			retInit += generateInitialAssignment(*varDecl);
		}
		t("returns", retParams.empty() ? "" : " -> " + retParams);
		t("initReturnVariables", retInit);
		t("body", generate(_function.body()));
		return t.render();
	});
}

string IRGenerator::generateGetter(VariableDeclaration const& _varDecl)
{
	string functionName = m_context.functionName(_varDecl);

	Type const* type = _varDecl.annotation().type;

	solAssert(!_varDecl.isConstant(), "");
	solAssert(!_varDecl.immutable(), "");
	solAssert(_varDecl.isStateVariable(), "");

	if (auto const* mappingType = dynamic_cast<MappingType const*>(type))
		return m_context.functionCollector().createFunction(functionName, [&]() {
			pair<u256, unsigned> slot_offset = m_context.storageLocationOfVariable(_varDecl);
			solAssert(slot_offset.second == 0, "");
			FunctionType funType(_varDecl);
			solUnimplementedAssert(funType.returnParameterTypes().size() == 1, "");
			TypePointer returnType = funType.returnParameterTypes().front();
			unsigned num_keys = 0;
			stringstream indexAccesses;
			string slot = m_context.newYulVariable();
			do
			{
				solUnimplementedAssert(
					mappingType->keyType()->sizeOnStack() == 1,
					"Multi-slot mapping key unimplemented - might not be a problem"
				);
				indexAccesses <<
					slot <<
					" := " <<
					m_utils.mappingIndexAccessFunction(*mappingType, *mappingType->keyType()) <<
					"(" <<
					slot;
				if (mappingType->keyType()->sizeOnStack() > 0)
					indexAccesses <<
						", " <<
						suffixedVariableNameList("key", num_keys, num_keys + mappingType->keyType()->sizeOnStack());
				indexAccesses << ")\n";
				num_keys += mappingType->keyType()->sizeOnStack();
			}
			while ((mappingType = dynamic_cast<MappingType const*>(mappingType->valueType())));

			return Whiskers(R"(
				function <functionName>(<keys>) -> rval {
					let <slot> := <base>
					<indexAccesses>
					rval := <readStorage>(<slot>)
				}
			)")
			("functionName", functionName)
			("keys", suffixedVariableNameList("key", 0, num_keys))
			("readStorage", m_utils.readFromStorage(*returnType, 0, false))
			("indexAccesses", indexAccesses.str())
			("slot", slot)
			("base", slot_offset.first.str())
			.render();
		});
	else
	{
		solUnimplementedAssert(type->isValueType(), "");

		return m_context.functionCollector().createFunction(functionName, [&]() {
			pair<u256, unsigned> slot_offset = m_context.storageLocationOfVariable(_varDecl);

			return Whiskers(R"(
				function <functionName>() -> rval {
					rval := <readStorage>(<slot>)
				}
			)")
			("functionName", functionName)
			("readStorage", m_utils.readFromStorage(*type, slot_offset.second, false))
			("slot", slot_offset.first.str())
			.render();
		});
	}
}

string IRGenerator::generateInitialAssignment(VariableDeclaration const& _varDecl)
{
	IRGeneratorForStatements generator(m_context, m_utils);
	generator.initializeLocalVar(_varDecl);
	return generator.code();
}

pair<string, map<ContractDefinition const*, string>> IRGenerator::evaluateConstructorArguments(
	ContractDefinition const& _contract
)
{
	map<ContractDefinition const*, string> constructorParams;
	vector<pair<ContractDefinition const*, std::vector<ASTPointer<Expression>>const *>> baseConstructorArguments;

	for (ASTPointer<InheritanceSpecifier> const& base: _contract.baseContracts())
		if (FunctionDefinition const* baseConstructor = dynamic_cast<ContractDefinition const*>(
					base->name().annotation().referencedDeclaration
			)->constructor(); baseConstructor && base->arguments())
			baseConstructorArguments.emplace_back(
					dynamic_cast<ContractDefinition const*>(baseConstructor->scope()),
					base->arguments()
			);

	if (FunctionDefinition const* constructor = _contract.constructor())
		for (auto const& modifier: constructor->modifiers())
			if (FunctionDefinition const* baseConstructor = dynamic_cast<ContractDefinition const*>(
				modifier->name()->annotation().referencedDeclaration
			)->constructor(); baseConstructor && modifier->arguments())
				baseConstructorArguments.emplace_back(
					dynamic_cast<ContractDefinition const*>(baseConstructor->scope()),
					modifier->arguments()
				);

	IRGeneratorForStatements generator{m_context, m_utils};
	for (auto&& [baseContract, arguments]: baseConstructorArguments)
	{
		solAssert(baseContract && arguments, "");
		if (baseContract->constructor() && !arguments->empty())
		{
			vector<string> params;
			for (size_t i = 0; i < arguments->size(); ++i)
				params.emplace_back(generator.evaluateExpression(
						*(arguments->at(i)),
						*(baseContract->constructor()->parameters()[i]->type())
				).commaSeparatedList());
			constructorParams[baseContract] = joinHumanReadable(params);
		}
	}

	return {generator.code(), constructorParams};
}

string IRGenerator::initStateVariables(ContractDefinition const& _contract)
{
	IRGeneratorForStatements generator{m_context, m_utils};
	for (VariableDeclaration const* variable: _contract.stateVariables())
		if (!variable->isConstant() && !variable->immutable())
			generator.initializeStateVar(*variable);

	return generator.code();
}


void IRGenerator::generateImplicitConstructors(ContractDefinition const& _contract)
{
	auto listAllParams = [&](
		map<ContractDefinition const*, string> const& baseParams) -> string
	{
		vector<string> params;
		for (ContractDefinition const* contract: _contract.annotation().linearizedBaseContracts)
			if (baseParams.count(contract))
				params.emplace_back(baseParams.at(contract));
		return joinHumanReadable(params);
	};

	map<ContractDefinition const*, string> baseConstructorParams;
	for (size_t i = 0; i < _contract.annotation().linearizedBaseContracts.size(); ++i)
	{
		ContractDefinition const* contract = _contract.annotation().linearizedBaseContracts[i];
		baseConstructorParams.erase(contract);

		m_context.functionCollector().createFunction(implicitConstructorName(*contract), [&]() {
			Whiskers t(R"(
				function <functionName>(<params><comma><baseParams>) {
					<evalBaseArguments>
					<?hasNextConstructor> <nextConstructor>(<nextParams>) </hasNextConstructor>
					<initStateVariables>
					<userDefinedConstructorBody>
				}
			)");
			string params;
			if (contract->constructor())
				for (ASTPointer<VariableDeclaration> const& varDecl: contract->constructor()->parameters())
					params += (params.empty() ? "" : ", ") + m_context.addLocalVariable(*varDecl).commaSeparatedList();
			t("params", params);
			string baseParamsString = listAllParams(baseConstructorParams);
			t("baseParams", baseParamsString);
			t("comma", !params.empty() && !baseParamsString.empty() ? ", " : "");
			t("functionName", implicitConstructorName(*contract));
			pair<string, map<ContractDefinition const*, string>> evaluatedArgs = evaluateConstructorArguments(*contract);
			baseConstructorParams.insert(evaluatedArgs.second.begin(), evaluatedArgs.second.end());
			t("evalBaseArguments", evaluatedArgs.first);
			if (i < _contract.annotation().linearizedBaseContracts.size() - 1)
			{
				t("hasNextConstructor", true);
				ContractDefinition const* nextContract = _contract.annotation().linearizedBaseContracts[i + 1];
				t("nextConstructor", implicitConstructorName(*nextContract));
				t("nextParams", listAllParams(baseConstructorParams));
			}
			else
				t("hasNextConstructor", false);
			t("initStateVariables", initStateVariables(*contract));
			t("userDefinedConstructorBody", contract->constructor() ? generate(contract->constructor()->body()) : "");

			return t.render();
		});
	}
}

string IRGenerator::deployCode(ContractDefinition const& _contract)
{
	Whiskers t(R"X(
		codecopy(0, dataoffset("<object>"), datasize("<object>"))
		return(0, datasize("<object>"))
	)X");
	t("object", m_context.runtimeObjectName(_contract));
	return t.render();
}

string IRGenerator::callValueCheck()
{
	return "if callvalue() { revert(0, 0) }";
}

string IRGenerator::implicitConstructorName(ContractDefinition const& _contract)
{
	return "constructor_" + _contract.name() + "_" + to_string(_contract.id());
}

string IRGenerator::dispatchRoutine(ContractDefinition const& _contract)
{
	Whiskers t(R"X(
		if iszero(lt(calldatasize(), 4))
		{
			let selector := <shr224>(calldataload(0))
			switch selector
			<#cases>
			case <functionSelector>
			{
				// <functionName>
				<callValueCheck>
				<assignToParams> <abiDecode>(4, calldatasize())
				<assignToRetParams> <function>(<params>)
				let memPos := <allocate>(0)
				let memEnd := <abiEncode>(memPos <comma> <retParams>)
				return(memPos, sub(memEnd, memPos))
			}
			</cases>
			default {}
		}
		if iszero(calldatasize()) { <receiveEther> }
		<fallback>
	)X");
	t("shr224", m_utils.shiftRightFunction(224));
	vector<map<string, string>> functions;
	for (auto const& function: _contract.interfaceFunctions())
	{
		functions.emplace_back();
		map<string, string>& templ = functions.back();
		templ["functionSelector"] = "0x" + function.first.hex();
		FunctionTypePointer const& type = function.second;
		templ["functionName"] = type->externalSignature();
		templ["callValueCheck"] = type->isPayable() ? "" : callValueCheck();

		unsigned paramVars = make_shared<TupleType>(type->parameterTypes())->sizeOnStack();
		unsigned retVars = make_shared<TupleType>(type->returnParameterTypes())->sizeOnStack();
		templ["assignToParams"] = paramVars == 0 ? "" : "let " + suffixedVariableNameList("param_", 0, paramVars) + " := ";
		templ["assignToRetParams"] = retVars == 0 ? "" : "let " + suffixedVariableNameList("ret_", 0, retVars) + " := ";

		ABIFunctions abiFunctions(m_evmVersion, m_context.revertStrings(), m_context.functionCollector());
		templ["abiDecode"] = abiFunctions.tupleDecoder(type->parameterTypes());
		templ["params"] = suffixedVariableNameList("param_", 0, paramVars);
		templ["retParams"] = suffixedVariableNameList("ret_", retVars, 0);

		if (FunctionDefinition const* funDef = dynamic_cast<FunctionDefinition const*>(&type->declaration()))
			templ["function"] = m_context.enqueueFunctionForCodeGeneration(*funDef);
		else if (VariableDeclaration const* varDecl = dynamic_cast<VariableDeclaration const*>(&type->declaration()))
			templ["function"] = generateGetter(*varDecl);
		else
			solAssert(false, "Unexpected declaration for function!");

		templ["allocate"] = m_utils.allocationFunction();
		templ["abiEncode"] = abiFunctions.tupleEncoder(type->returnParameterTypes(), type->returnParameterTypes(), false);
		templ["comma"] = retVars == 0 ? "" : ", ";
	}
	t("cases", functions);
	if (FunctionDefinition const* fallback = _contract.fallbackFunction())
	{
		string fallbackCode;
		if (!fallback->isPayable())
			fallbackCode += callValueCheck();
		fallbackCode += m_context.enqueueFunctionForCodeGeneration(*fallback) + "() stop()";

		t("fallback", fallbackCode);
	}
	else
		t("fallback", "revert(0, 0)");
	if (FunctionDefinition const* etherReceiver = _contract.receiveFunction())
		t("receiveEther", m_context.enqueueFunctionForCodeGeneration(*etherReceiver) + "() stop()");
	else
		t("receiveEther", "");
	return t.render();
}

string IRGenerator::memoryInit()
{
	// This function should be called at the beginning of the EVM call frame
	// and thus can assume all memory to be zero, including the contents of
	// the "zero memory area" (the position CompilerUtils::zeroPointer points to).
	return
		Whiskers{"mstore(<memPtr>, <generalPurposeStart>)"}
		("memPtr", to_string(CompilerUtils::freeMemoryPointer))
		("generalPurposeStart", to_string(CompilerUtils::generalPurposeMemoryStart))
		.render();
}

void IRGenerator::resetContext(ContractDefinition const& _contract)
{
	solAssert(
		m_context.functionGenerationQueueEmpty(),
		"Reset function generation queue while it still had functions."
	);
	solAssert(
		m_context.functionCollector().requestedFunctions().empty(),
		"Reset context while it still had functions."
	);
	m_context = IRGenerationContext(m_evmVersion, m_context.revertStrings(), m_optimiserSettings);

	m_context.setMostDerivedContract(_contract);
	for (auto const& var: ContractType(_contract).stateVariables())
		m_context.addStateVariable(*get<0>(var), get<1>(var), get<2>(var));
}
