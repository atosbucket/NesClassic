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

#include <test/libyul/YulOptimizerTestCommon.h>

#include <libyul/optimiser/BlockFlattener.h>
#include <libyul/optimiser/VarDeclInitializer.h>
#include <libyul/optimiser/VarNameCleaner.h>
#include <libyul/optimiser/ControlFlowSimplifier.h>
#include <libyul/optimiser/DeadCodeEliminator.h>
#include <libyul/optimiser/Disambiguator.h>
#include <libyul/optimiser/CircularReferencesPruner.h>
#include <libyul/optimiser/ConditionalUnsimplifier.h>
#include <libyul/optimiser/ConditionalSimplifier.h>
#include <libyul/optimiser/CommonSubexpressionEliminator.h>
#include <libyul/optimiser/EquivalentFunctionCombiner.h>
#include <libyul/optimiser/ExpressionSplitter.h>
#include <libyul/optimiser/FunctionGrouper.h>
#include <libyul/optimiser/FunctionHoister.h>
#include <libyul/optimiser/ExpressionInliner.h>
#include <libyul/optimiser/FullInliner.h>
#include <libyul/optimiser/ForLoopConditionIntoBody.h>
#include <libyul/optimiser/ForLoopInitRewriter.h>
#include <libyul/optimiser/LoadResolver.h>
#include <libyul/optimiser/LoopInvariantCodeMotion.h>
#include <libyul/optimiser/MainFunction.h>
#include <libyul/optimiser/StackLimitEvader.h>
#include <libyul/optimiser/NameDisplacer.h>
#include <libyul/optimiser/Rematerialiser.h>
#include <libyul/optimiser/ExpressionSimplifier.h>
#include <libyul/optimiser/UnusedFunctionParameterPruner.h>
#include <libyul/optimiser/UnusedPruner.h>
#include <libyul/optimiser/ExpressionJoiner.h>
#include <libyul/optimiser/OptimiserStep.h>
#include <libyul/optimiser/ReasoningBasedSimplifier.h>
#include <libyul/optimiser/SSAReverser.h>
#include <libyul/optimiser/SSATransform.h>
#include <libyul/optimiser/Semantics.h>
#include <libyul/optimiser/RedundantAssignEliminator.h>
#include <libyul/optimiser/StructuralSimplifier.h>
#include <libyul/optimiser/StackCompressor.h>
#include <libyul/optimiser/Suite.h>
#include <libyul/backends/evm/ConstantOptimiser.h>
#include <libyul/backends/evm/EVMDialect.h>
#include <libyul/backends/evm/EVMMetrics.h>
#include <libyul/backends/wasm/WordSizeTransform.h>
#include <libyul/backends/wasm/WasmDialect.h>
#include <libyul/AsmPrinter.h>
#include <libyul/AsmAnalysis.h>
#include <libyul/CompilabilityChecker.h>

#include <libsolidity/interface/OptimiserSettings.h>

#include <variant>

using namespace solidity;
using namespace solidity::util;
using namespace solidity::langutil;
using namespace solidity::yul;
using namespace solidity::yul::test;
using namespace solidity::frontend;
using namespace std;

YulOptimizerTester::YulOptimizerTester(
	shared_ptr<Object> _obj,
	Dialect const& _dialect,
	string const& _optimizerStep,
	bool _fuzzerMode
)
{
	m_object = _obj;
	m_ast = m_object->code;
	m_analysisInfo = m_object->analysisInfo;
	m_dialect = &_dialect;
	m_optimizerStep = _optimizerStep;
	m_fuzzerMode = _fuzzerMode;
}

bool YulOptimizerTester::runStep()
{
	yulAssert(m_dialect, "Dialect not set.");

	updateContext();

	if (m_optimizerStep == "disambiguator")
		disambiguate();
	else if (m_optimizerStep == "nameDisplacer")
	{
		disambiguate();
		NameDisplacer{
			*m_nameDispenser,
			{"illegal1"_yulstring, "illegal2"_yulstring, "illegal3"_yulstring, "illegal4"_yulstring, "illegal5"_yulstring}
		}(*m_ast);
	}
	else if (m_optimizerStep == "blockFlattener")
	{
		disambiguate();
		BlockFlattener::run(*m_context, *m_ast);
	}
	else if (m_optimizerStep == "constantOptimiser")
	{
		GasMeter meter(dynamic_cast<EVMDialect const&>(*m_dialect), false, 200);
		ConstantOptimiser{dynamic_cast<EVMDialect const&>(*m_dialect), meter}(*m_ast);
	}
	else if (m_optimizerStep == "varDeclInitializer")
		VarDeclInitializer::run(*m_context, *m_ast);
	else if (m_optimizerStep == "varNameCleaner")
	{
		disambiguate();
		if (m_fuzzerMode)
			FunctionHoister::run(*m_context, *m_ast);
		FunctionGrouper::run(*m_context, *m_ast);
		VarNameCleaner::run(*m_context, *m_ast);
	}
	else if (m_optimizerStep == "forLoopConditionIntoBody")
	{
		disambiguate();
		ForLoopConditionIntoBody::run(*m_context, *m_ast);
	}
	else if (m_optimizerStep == "forLoopInitRewriter")
	{
		disambiguate();
		ForLoopInitRewriter::run(*m_context, *m_ast);
	}
	else if (m_optimizerStep == "commonSubexpressionEliminator")
	{
		disambiguate();
		if (m_fuzzerMode)
		{
			ForLoopInitRewriter::run(*m_context, *m_ast);
			FunctionHoister::run(*m_context, *m_ast);
		}
		CommonSubexpressionEliminator::run(*m_context, *m_ast);
	}
	else if (m_optimizerStep == "conditionalUnsimplifier")
	{
		disambiguate();
		ConditionalUnsimplifier::run(*m_context, *m_ast);
	}
	else if (m_optimizerStep == "conditionalSimplifier")
	{
		disambiguate();
		ConditionalSimplifier::run(*m_context, *m_ast);
	}
	else if (m_optimizerStep == "expressionSplitter")
		ExpressionSplitter::run(*m_context, *m_ast);
	else if (m_optimizerStep == "expressionJoiner")
	{
		disambiguate();
		ExpressionJoiner::run(*m_context, *m_ast);
	}
	else if (m_optimizerStep == "splitJoin")
	{
		disambiguate();
		ExpressionSplitter::run(*m_context, *m_ast);
		ExpressionJoiner::run(*m_context, *m_ast);
		ExpressionJoiner::run(*m_context, *m_ast);
	}
	else if (m_optimizerStep == "functionGrouper")
	{
		disambiguate();
		FunctionGrouper::run(*m_context, *m_ast);
	}
	else if (m_optimizerStep == "functionHoister")
	{
		disambiguate();
		FunctionHoister::run(*m_context, *m_ast);
	}
	else if (m_optimizerStep == "expressionInliner")
	{
		disambiguate();
		ExpressionInliner::run(*m_context, *m_ast);
	}
	else if (m_optimizerStep == "fullInliner")
	{
		disambiguate();
		FunctionHoister::run(*m_context, *m_ast);
		FunctionGrouper::run(*m_context, *m_ast);
		ExpressionSplitter::run(*m_context, *m_ast);
		FullInliner::run(*m_context, *m_ast);
		ExpressionJoiner::run(*m_context, *m_ast);
	}
	else if (m_optimizerStep == "mainFunction")
	{
		disambiguate();
		FunctionGrouper::run(*m_context, *m_ast);
		MainFunction::run(*m_context, *m_ast);
	}
	else if (m_optimizerStep == "rematerialiser")
	{
		disambiguate();
		if (m_fuzzerMode)
		{
			ForLoopInitRewriter::run(*m_context, *m_ast);
			FunctionHoister::run(*m_context, *m_ast);
		}
		Rematerialiser::run(*m_context, *m_ast);
	}
	else if (m_optimizerStep == "expressionSimplifier")
	{
		disambiguate();
		if (m_fuzzerMode)
		{
			ForLoopInitRewriter::run(*m_context, *m_ast);
			FunctionHoister::run(*m_context, *m_ast);
		}
		ExpressionSplitter::run(*m_context, *m_ast);
		CommonSubexpressionEliminator::run(*m_context, *m_ast);
		ExpressionSimplifier::run(*m_context, *m_ast);
		ExpressionSimplifier::run(*m_context, *m_ast);
		ExpressionSimplifier::run(*m_context, *m_ast);
		UnusedPruner::run(*m_context, *m_ast);
		ExpressionJoiner::run(*m_context, *m_ast);
		ExpressionJoiner::run(*m_context, *m_ast);
	}
	else if (m_optimizerStep == "fullSimplify")
	{
		disambiguate();
		ExpressionSplitter::run(*m_context, *m_ast);
		ForLoopInitRewriter::run(*m_context, *m_ast);
		if (m_fuzzerMode)
			FunctionHoister::run(*m_context, *m_ast);
		CommonSubexpressionEliminator::run(*m_context, *m_ast);
		ExpressionSimplifier::run(*m_context, *m_ast);
		UnusedPruner::run(*m_context, *m_ast);
		CircularReferencesPruner::run(*m_context, *m_ast);
		DeadCodeEliminator::run(*m_context, *m_ast);
		ExpressionJoiner::run(*m_context, *m_ast);
		ExpressionJoiner::run(*m_context, *m_ast);
	}
	else if (m_optimizerStep == "unusedFunctionParameterPruner")
	{
		disambiguate();
		if (m_fuzzerMode)
			ForLoopInitRewriter::run(*m_context, *m_ast);
		FunctionHoister::run(*m_context, *m_object->code);
		LiteralRematerialiser::run(*m_context, *m_object->code);
		UnusedFunctionParameterPruner::run(*m_context, *m_object->code);
	}
	else if (m_optimizerStep == "unusedPruner")
	{
		disambiguate();
		UnusedPruner::run(*m_context, *m_ast);
	}
	else if (m_optimizerStep == "circularReferencesPruner")
	{
		disambiguate();
		FunctionHoister::run(*m_context, *m_ast);
		CircularReferencesPruner::run(*m_context, *m_ast);
	}
	else if (m_optimizerStep == "deadCodeEliminator")
	{
		disambiguate();
		ForLoopInitRewriter::run(*m_context, *m_ast);
		DeadCodeEliminator::run(*m_context, *m_ast);
	}
	else if (m_optimizerStep == "ssaTransform")
	{
		disambiguate();
		ForLoopInitRewriter::run(*m_context, *m_ast);
		SSATransform::run(*m_context, *m_ast);
	}
	else if (m_optimizerStep == "redundantAssignEliminator")
	{
		disambiguate();
		ForLoopInitRewriter::run(*m_context, *m_ast);
		RedundantAssignEliminator::run(*m_context, *m_ast);
	}
	else if (m_optimizerStep == "ssaPlusCleanup")
	{
		disambiguate();
		if (m_fuzzerMode)
			ForLoopInitRewriter::run(*m_context, *m_ast);
		SSATransform::run(*m_context, *m_ast);
		RedundantAssignEliminator::run(*m_context, *m_ast);
	}
	else if (m_optimizerStep == "loadResolver")
	{
		disambiguate();
		ForLoopInitRewriter::run(*m_context, *m_ast);
		if (m_fuzzerMode)
			FunctionHoister::run(*m_context, *m_ast);
		ExpressionSplitter::run(*m_context, *m_ast);
		CommonSubexpressionEliminator::run(*m_context, *m_ast);
		ExpressionSimplifier::run(*m_context, *m_ast);

		LoadResolver::run(*m_context, *m_ast);

		UnusedPruner::run(*m_context, *m_ast);
		ExpressionJoiner::run(*m_context, *m_ast);
		ExpressionJoiner::run(*m_context, *m_ast);
	}
	else if (m_optimizerStep == "loopInvariantCodeMotion")
	{
		disambiguate();
		ForLoopInitRewriter::run(*m_context, *m_ast);
		if (m_fuzzerMode)
			FunctionHoister::run(*m_context, *m_ast);
		LoopInvariantCodeMotion::run(*m_context, *m_ast);
	}
	else if (m_optimizerStep == "controlFlowSimplifier")
	{
		disambiguate();
		if (m_fuzzerMode)
			ForLoopInitRewriter::run(*m_context, *m_ast);
		ControlFlowSimplifier::run(*m_context, *m_ast);
	}
	else if (m_optimizerStep == "structuralSimplifier")
	{
		disambiguate();
		ForLoopInitRewriter::run(*m_context, *m_ast);
		if (m_fuzzerMode)
			FunctionHoister::run(*m_context, *m_ast);
		LiteralRematerialiser::run(*m_context, *m_ast);
		StructuralSimplifier::run(*m_context, *m_ast);
	}
	else if (m_optimizerStep == "reasoningBasedSimplifier")
	{
		disambiguate();
		ReasoningBasedSimplifier::run(*m_context, *m_object->code);
	}
	else if (m_optimizerStep == "equivalentFunctionCombiner")
	{
		disambiguate();
		if (m_fuzzerMode)
		{
			ForLoopInitRewriter::run(*m_context, *m_ast);
			FunctionHoister::run(*m_context, *m_ast);
		}
		EquivalentFunctionCombiner::run(*m_context, *m_ast);
	}
	else if (m_optimizerStep == "ssaReverser")
	{
		disambiguate();
		SSAReverser::run(*m_context, *m_ast);
	}
	else if (m_optimizerStep == "ssaAndBack")
	{
		disambiguate();
		if (m_fuzzerMode)
			ForLoopInitRewriter::run(*m_context, *m_ast);
		// apply SSA
		SSATransform::run(*m_context, *m_ast);
		RedundantAssignEliminator::run(*m_context, *m_ast);
		// reverse SSA
		SSAReverser::run(*m_context, *m_ast);
		if (m_fuzzerMode)
			FunctionHoister::run(*m_context, *m_ast);
		CommonSubexpressionEliminator::run(*m_context, *m_ast);
		UnusedPruner::run(*m_context, *m_ast);
	}
	else if (m_optimizerStep == "stackCompressor")
	{
		disambiguate();
		if (m_fuzzerMode)
		{
			ForLoopInitRewriter::run(*m_context, *m_ast);
			FunctionHoister::run(*m_context, *m_ast);
		}
		FunctionGrouper::run(*m_context, *m_ast);
		size_t maxIterations = 16;
		StackCompressor::run(*m_dialect, *m_object, true, maxIterations);
		BlockFlattener::run(*m_context, *m_ast);
	}
	else if (m_optimizerStep == "wordSizeTransform")
	{
		disambiguate();
		ExpressionSplitter::run(*m_context, *m_ast);
		WordSizeTransform::run(*m_dialect, *m_dialect, *m_ast, *m_nameDispenser);
	}
	else if (m_optimizerStep == "fullSuite")
	{
		GasMeter meter(dynamic_cast<EVMDialect const&>(*m_dialect), false, 200);
		OptimiserSuite::run(*m_dialect, &meter, *m_object, true, solidity::frontend::OptimiserSettings::DefaultYulOptimiserSteps);
	}
	else if (m_optimizerStep == "stackLimitEvader")
	{
		disambiguate();
		StackLimitEvader::run(*m_context, *m_object, CompilabilityChecker{
			*m_dialect,
			*m_object,
			true
		}.unreachableVariables);
	}
	else if (m_optimizerStep == "fakeStackLimitEvader")
	{
		disambiguate();
		// Mark all variables with a name starting with "$" for escalation to memory.
		struct FakeUnreachableGenerator: ASTWalker
		{
			map<YulString, set<YulString>> fakeUnreachables;
			using ASTWalker::operator();
			void operator()(FunctionDefinition const& _function) override
			{
				YulString originalFunctionName = m_currentFunction;
				m_currentFunction = _function.name;
				ASTWalker::operator()(_function);
				m_currentFunction = originalFunctionName;
			}
			void visitVariableName(YulString _var)
			{
				if (!_var.empty() && _var.str().front() == '$')
					fakeUnreachables[m_currentFunction].insert(_var);
			}
			void operator()(VariableDeclaration const& _varDecl) override
			{
				for (auto const& var: _varDecl.variables)
					visitVariableName(var.name);
				ASTWalker::operator()(_varDecl);
			}
			void operator()(Identifier const& _identifier) override
			{
				visitVariableName(_identifier.name);
				ASTWalker::operator()(_identifier);
			}
			YulString m_currentFunction = YulString{};
		};
		FakeUnreachableGenerator fakeUnreachableGenerator;
		fakeUnreachableGenerator(*m_ast);
		StackLimitEvader::run(*m_context, *m_object, fakeUnreachableGenerator.fakeUnreachables);
	}
	else
		return false;

	return true;
}

shared_ptr<Block> YulOptimizerTester::run()
{
	bool success = runStep();
	if (!success)
		return nullptr;
	else
		return m_ast;
}

void YulOptimizerTester::disambiguate()
{
	*m_object->code = std::get<Block>(Disambiguator(*m_dialect, *m_analysisInfo)(*m_object->code));
	m_analysisInfo.reset();
	updateContext();
}

void YulOptimizerTester::updateContext()
{
	m_nameDispenser = make_unique<NameDispenser>(*m_dialect, *m_object->code, m_reservedIdentifiers);
	m_context = make_unique<OptimiserStepContext>(OptimiserStepContext{
		*m_dialect,
		*m_nameDispenser,
		m_reservedIdentifiers
	});
}
