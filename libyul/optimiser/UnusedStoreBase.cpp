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
 * Base class for both UnusedAssignEliminator and UnusedStoreEliminator.
 */

#include <libyul/optimiser/UnusedStoreBase.h>

#include <libyul/optimiser/Semantics.h>
#include <libyul/optimiser/OptimiserStep.h>
#include <libyul/AST.h>

#include <libsolutil/CommonData.h>

#include <range/v3/action/remove_if.hpp>

using namespace std;
using namespace solidity;
using namespace solidity::yul;

void UnusedStoreBase::operator()(If const& _if)
{
	visit(*_if.condition);

	TrackedStores skipBranch{m_stores};
	(*this)(_if.body);

	merge(m_stores, move(skipBranch));
}

void UnusedStoreBase::operator()(Switch const& _switch)
{
	visit(*_switch.expression);

	TrackedStores const preState{m_stores};

	bool hasDefault = false;
	vector<TrackedStores> branches;
	for (auto const& c: _switch.cases)
	{
		if (!c.value)
			hasDefault = true;
		(*this)(c.body);
		branches.emplace_back(move(m_stores));
		m_stores = preState;
	}

	if (hasDefault)
	{
		m_stores = move(branches.back());
		branches.pop_back();
	}
	for (auto& branch: branches)
		merge(m_stores, move(branch));
}

void UnusedStoreBase::operator()(FunctionDefinition const& _functionDefinition)
{
	ScopedSaveAndRestore outerAssignments(m_stores, {});
	ScopedSaveAndRestore forLoopInfo(m_forLoopInfo, {});
	ScopedSaveAndRestore forLoopNestingDepth(m_forLoopNestingDepth, 0);

	(*this)(_functionDefinition.body);

	finalizeFunctionDefinition(_functionDefinition);
}

void UnusedStoreBase::operator()(ForLoop const& _forLoop)
{
	ScopedSaveAndRestore outerForLoopInfo(m_forLoopInfo, {});
	ScopedSaveAndRestore forLoopNestingDepth(m_forLoopNestingDepth, m_forLoopNestingDepth + 1);

	// If the pre block was not empty,
	// we would have to deal with more complicated scoping rules.
	assertThrow(_forLoop.pre.statements.empty(), OptimizerException, "");

	// We just run the loop twice to account for the back edge.
	// There need not be more runs because we only have three different states.

	visit(*_forLoop.condition);

	TrackedStores zeroRuns{m_stores};

	(*this)(_forLoop.body);
	merge(m_stores, move(m_forLoopInfo.pendingContinueStmts));
	m_forLoopInfo.pendingContinueStmts = {};
	(*this)(_forLoop.post);

	visit(*_forLoop.condition);

	if (m_forLoopNestingDepth < 6)
	{
		// Do the second run only for small nesting depths to avoid horrible runtime.
		TrackedStores oneRun{m_stores};

		(*this)(_forLoop.body);

		merge(m_stores, move(m_forLoopInfo.pendingContinueStmts));
		m_forLoopInfo.pendingContinueStmts.clear();
		(*this)(_forLoop.post);

		visit(*_forLoop.condition);
		// Order of merging does not matter because "max" is commutative and associative.
		merge(m_stores, move(oneRun));
	}
	else
		// Shortcut to avoid horrible runtime.
		shortcutNestedLoop(zeroRuns);

	// Order of merging does not matter because "max" is commutative and associative.
	merge(m_stores, move(zeroRuns));
	merge(m_stores, move(m_forLoopInfo.pendingBreakStmts));
	m_forLoopInfo.pendingBreakStmts.clear();
}

void UnusedStoreBase::operator()(Break const&)
{
	m_forLoopInfo.pendingBreakStmts.emplace_back(move(m_stores));
	m_stores.clear();
}

void UnusedStoreBase::operator()(Continue const&)
{
	m_forLoopInfo.pendingContinueStmts.emplace_back(move(m_stores));
	m_stores.clear();
}

void UnusedStoreBase::merge(TrackedStores& _target, TrackedStores&& _other)
{
	util::joinMap(_target, move(_other), [](
		map<Statement const*, State>& _assignmentHere,
		map<Statement const*, State>&& _assignmentThere
	)
	{
		return util::joinMap(_assignmentHere, move(_assignmentThere), State::join);
	});
}

void UnusedStoreBase::merge(TrackedStores& _target, vector<TrackedStores>&& _source)
{
	for (TrackedStores& ts: _source)
		merge(_target, move(ts));
	_source.clear();
}
