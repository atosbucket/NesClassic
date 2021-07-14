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
 * Full assembly stack that can support EVM-assembly and Yul as input and EVM, EVM1.5 and
 * Ewasm as output.
 */


#include <libyul/AssemblyStack.h>

#include <libyul/AsmAnalysis.h>
#include <libyul/AsmAnalysisInfo.h>
#include <libyul/backends/evm/EthAssemblyAdapter.h>
#include <libyul/backends/evm/EVMCodeTransform.h>
#include <libyul/backends/evm/EVMDialect.h>
#include <libyul/backends/evm/EVMObjectCompiler.h>
#include <libyul/backends/evm/EVMMetrics.h>
#include <libyul/backends/wasm/WasmDialect.h>
#include <libyul/backends/wasm/WasmObjectCompiler.h>
#include <libyul/backends/wasm/EVMToEwasmTranslator.h>
#include <libyul/ObjectParser.h>
#include <libyul/optimiser/Suite.h>

#include <libevmasm/Assembly.h>
#include <liblangutil/Scanner.h>
#include <optional>

using namespace std;
using namespace solidity;
using namespace solidity::yul;
using namespace solidity::langutil;

namespace
{
Dialect const& languageToDialect(AssemblyStack::Language _language, EVMVersion _version)
{
	switch (_language)
	{
	case AssemblyStack::Language::Assembly:
	case AssemblyStack::Language::StrictAssembly:
		return EVMDialect::strictAssemblyForEVMObjects(_version);
	case AssemblyStack::Language::Yul:
		return EVMDialectTyped::instance(_version);
	case AssemblyStack::Language::Ewasm:
		return WasmDialect::instance();
	}
	yulAssert(false, "");
	return Dialect::yulDeprecated();
}

// Duplicated from libsolidity/codegen/CompilerContext.cpp
// TODO: refactor and remove duplication
evmasm::Assembly::OptimiserSettings translateOptimiserSettings(
	frontend::OptimiserSettings const& _settings,
	langutil::EVMVersion _evmVersion
)
{
	// Constructing it this way so that we notice changes in the fields.
	evmasm::Assembly::OptimiserSettings asmSettings{false, false,  false, false, false, false, false, _evmVersion, 0};
	asmSettings.isCreation = true;
	asmSettings.runInliner = _settings.runInliner;
	asmSettings.runJumpdestRemover = _settings.runJumpdestRemover;
	asmSettings.runPeephole = _settings.runPeephole;
	asmSettings.runDeduplicate = _settings.runDeduplicate;
	asmSettings.runCSE = _settings.runCSE;
	asmSettings.runConstantOptimiser = _settings.runConstantOptimiser;
	asmSettings.expectedExecutionsPerDeployment = _settings.expectedExecutionsPerDeployment;
	asmSettings.evmVersion = _evmVersion;

	return asmSettings;
}

}


CharStream const& AssemblyStack::charStream(string const& _sourceName) const
{
	yulAssert(m_charStream, "");
	yulAssert(m_charStream->name() == _sourceName, "");
	return *m_charStream;
}

bool AssemblyStack::parseAndAnalyze(std::string const& _sourceName, std::string const& _source)
{
	m_errors.clear();
	m_analysisSuccessful = false;
	m_charStream = make_unique<CharStream>(_source, _sourceName);
	shared_ptr<Scanner> scanner = make_shared<Scanner>(*m_charStream);
	m_parserResult = ObjectParser(m_errorReporter, languageToDialect(m_language, m_evmVersion)).parse(scanner, false);
	if (!m_errorReporter.errors().empty())
		return false;
	yulAssert(m_parserResult, "");
	yulAssert(m_parserResult->code, "");

	return analyzeParsed();
}

void AssemblyStack::optimize()
{
	if (!m_optimiserSettings.runYulOptimiser)
		return;

	yulAssert(m_analysisSuccessful, "Analysis was not successful.");

	m_analysisSuccessful = false;
	yulAssert(m_parserResult, "");
	optimize(*m_parserResult, true);
	yulAssert(analyzeParsed(), "Invalid source code after optimization.");
}

void AssemblyStack::translate(AssemblyStack::Language _targetLanguage)
{
	if (m_language == _targetLanguage)
		return;

	yulAssert(
		m_language == Language::StrictAssembly && _targetLanguage == Language::Ewasm,
		"Invalid language combination"
	);

	*m_parserResult = EVMToEwasmTranslator(
		languageToDialect(m_language, m_evmVersion),
		*this
	).run(*parserResult());

	m_language = _targetLanguage;
}

bool AssemblyStack::analyzeParsed()
{
	yulAssert(m_parserResult, "");
	m_analysisSuccessful = analyzeParsed(*m_parserResult);
	return m_analysisSuccessful;
}

bool AssemblyStack::analyzeParsed(Object& _object)
{
	yulAssert(_object.code, "");
	_object.analysisInfo = make_shared<AsmAnalysisInfo>();

	AsmAnalyzer analyzer(
		*_object.analysisInfo,
		m_errorReporter,
		languageToDialect(m_language, m_evmVersion),
		{},
		_object.qualifiedDataNames()
	);
	bool success = analyzer.analyze(*_object.code);
	for (auto& subNode: _object.subObjects)
		if (auto subObject = dynamic_cast<Object*>(subNode.get()))
			if (!analyzeParsed(*subObject))
				success = false;
	return success;
}

void AssemblyStack::compileEVM(AbstractAssembly& _assembly, bool _optimize) const
{
	EVMDialect const* dialect = nullptr;
	switch (m_language)
	{
		case Language::Assembly:
		case Language::StrictAssembly:
			dialect = &EVMDialect::strictAssemblyForEVMObjects(m_evmVersion);
			break;
		case Language::Yul:
			dialect = &EVMDialectTyped::instance(m_evmVersion);
			break;
		default:
			yulAssert(false, "Invalid language.");
			break;
	}

	EVMObjectCompiler::compile(*m_parserResult, _assembly, *dialect, _optimize);
}

void AssemblyStack::optimize(Object& _object, bool _isCreation)
{
	yulAssert(_object.code, "");
	yulAssert(_object.analysisInfo, "");
	for (auto& subNode: _object.subObjects)
		if (auto subObject = dynamic_cast<Object*>(subNode.get()))
			optimize(*subObject, false);

	Dialect const& dialect = languageToDialect(m_language, m_evmVersion);
	unique_ptr<GasMeter> meter;
	if (EVMDialect const* evmDialect = dynamic_cast<EVMDialect const*>(&dialect))
		meter = make_unique<GasMeter>(*evmDialect, _isCreation, m_optimiserSettings.expectedExecutionsPerDeployment);
	OptimiserSuite::run(
		dialect,
		meter.get(),
		_object,
		m_optimiserSettings.optimizeStackAllocation,
		m_optimiserSettings.yulOptimiserSteps,
		_isCreation ? nullopt : make_optional(m_optimiserSettings.expectedExecutionsPerDeployment),
		{}
	);
}

MachineAssemblyObject AssemblyStack::assemble(Machine _machine) const
{
	yulAssert(m_analysisSuccessful, "");
	yulAssert(m_parserResult, "");
	yulAssert(m_parserResult->code, "");
	yulAssert(m_parserResult->analysisInfo, "");

	switch (_machine)
	{
	case Machine::EVM:
		return assembleWithDeployed().first;
	case Machine::Ewasm:
	{
		yulAssert(m_language == Language::Ewasm, "");
		Dialect const& dialect = languageToDialect(m_language, EVMVersion{});

		MachineAssemblyObject object;
		auto result = WasmObjectCompiler::compile(*m_parserResult, dialect);
		object.assembly = std::move(result.first);
		object.bytecode = make_shared<evmasm::LinkerObject>();
		object.bytecode->bytecode = std::move(result.second);
		return object;
	}
	}
	// unreachable
	return MachineAssemblyObject();
}

std::pair<MachineAssemblyObject, MachineAssemblyObject>
AssemblyStack::assembleWithDeployed(optional<string_view> _deployName) const
{
	auto [creationAssembly, deployedAssembly] = assembleEVMWithDeployed(_deployName);
	yulAssert(creationAssembly, "");
	yulAssert(m_charStream, "");

	MachineAssemblyObject creationObject;
	creationObject.bytecode = make_shared<evmasm::LinkerObject>(creationAssembly->assemble());
	yulAssert(creationObject.bytecode->immutableReferences.empty(), "Leftover immutables.");
	creationObject.assembly = creationAssembly->assemblyString();
	creationObject.sourceMappings = make_unique<string>(
		evmasm::AssemblyItem::computeSourceMapping(
			creationAssembly->items(),
			{{m_charStream->name(), 0}}
		)
	);

	MachineAssemblyObject deployedObject;
	if (deployedAssembly)
	{
		deployedObject.bytecode = make_shared<evmasm::LinkerObject>(deployedAssembly->assemble());
		deployedObject.assembly = deployedAssembly->assemblyString();
		deployedObject.sourceMappings = make_unique<string>(
			evmasm::AssemblyItem::computeSourceMapping(
				deployedAssembly->items(),
				{{m_charStream->name(), 0}}
			)
		);
	}

	return {std::move(creationObject), std::move(deployedObject)};
}

std::pair<std::shared_ptr<evmasm::Assembly>, std::shared_ptr<evmasm::Assembly>>
AssemblyStack::assembleEVMWithDeployed(optional<string_view> _deployName) const
{
	yulAssert(m_analysisSuccessful, "");
	yulAssert(m_parserResult, "");
	yulAssert(m_parserResult->code, "");
	yulAssert(m_parserResult->analysisInfo, "");

	evmasm::Assembly assembly;
	EthAssemblyAdapter adapter(assembly);
	compileEVM(adapter, m_optimiserSettings.optimizeStackAllocation);

	assembly.optimise(translateOptimiserSettings(m_optimiserSettings, m_evmVersion));

	optional<size_t> subIndex;

	// Pick matching assembly if name was given
	if (_deployName.has_value())
	{
		for (size_t i = 0; i < assembly.numSubs(); i++)
			if (assembly.sub(i).name() == _deployName)
			{
				subIndex = i;
				break;
			}

		solAssert(subIndex.has_value(), "Failed to find object to be deployed.");
	}
	// Otherwise use heuristic: If there is a single sub-assembly, this is likely the object to be deployed.
	else if (assembly.numSubs() == 1)
		subIndex = 0;

	if (subIndex.has_value())
	{
		evmasm::Assembly& runtimeAssembly = assembly.sub(*subIndex);
		return {make_shared<evmasm::Assembly>(assembly), make_shared<evmasm::Assembly>(runtimeAssembly)};
	}

	return {make_shared<evmasm::Assembly>(assembly), {}};
}

string AssemblyStack::print() const
{
	yulAssert(m_parserResult, "");
	yulAssert(m_parserResult->code, "");
	return m_parserResult->toString(&languageToDialect(m_language, m_evmVersion)) + "\n";
}

shared_ptr<Object> AssemblyStack::parserResult() const
{
	yulAssert(m_analysisSuccessful, "Analysis was not successful.");
	yulAssert(m_parserResult, "");
	yulAssert(m_parserResult->code, "");
	return m_parserResult;
}
