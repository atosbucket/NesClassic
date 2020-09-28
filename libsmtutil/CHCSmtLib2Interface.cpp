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

#include <libsmtutil/CHCSmtLib2Interface.h>

#include <libsolutil/Keccak256.h>

#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include <array>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>

using namespace std;
using namespace solidity;
using namespace solidity::util;
using namespace solidity::frontend;
using namespace solidity::smtutil;

CHCSmtLib2Interface::CHCSmtLib2Interface(
	map<h256, string> const& _queryResponses,
	ReadCallback::Callback const& _smtCallback
):
	m_smtlib2(make_unique<SMTLib2Interface>(_queryResponses, _smtCallback)),
	m_queryResponses(_queryResponses),
	m_smtCallback(_smtCallback)
{
	reset();
}

void CHCSmtLib2Interface::reset()
{
	m_accumulatedOutput.clear();
	m_variables.clear();
	m_unhandledQueries.clear();
}

void CHCSmtLib2Interface::registerRelation(Expression const& _expr)
{
	smtAssert(_expr.sort, "");
	smtAssert(_expr.sort->kind == Kind::Function, "");
	if (!m_variables.count(_expr.name))
	{
		auto fSort = dynamic_pointer_cast<FunctionSort>(_expr.sort);
		string domain = m_smtlib2->toSmtLibSort(fSort->domain);
		// Relations are predicates which have implicit codomain Bool.
		m_variables.insert(_expr.name);
		write(
			"(declare-rel |" +
			_expr.name +
			"| " +
			domain +
			")"
		);
	}
}

void CHCSmtLib2Interface::addRule(Expression const& _expr, std::string const& _name)
{
	write(
		"(rule (! " +
		m_smtlib2->toSExpr(_expr) +
		" :named " +
		_name +
		"))"
	);
}

pair<CheckResult, CHCSolverInterface::CexGraph> CHCSmtLib2Interface::query(Expression const& _block)
{
	string accumulated{};
	swap(m_accumulatedOutput, accumulated);
	for (auto const& var: m_smtlib2->variables())
		declareVariable(var.first, var.second);
	m_accumulatedOutput += accumulated;

	string response = querySolver(
		m_accumulatedOutput +
		"\n(query " + _block.name + " :print-certificate true)"
	);

	CheckResult result;
	// TODO proper parsing
	if (boost::starts_with(response, "sat\n"))
		result = CheckResult::SATISFIABLE;
	else if (boost::starts_with(response, "unsat\n"))
		result = CheckResult::UNSATISFIABLE;
	else if (boost::starts_with(response, "unknown\n"))
		result = CheckResult::UNKNOWN;
	else
		result = CheckResult::ERROR;

	// TODO collect invariants or counterexamples.
	return {result, {}};
}

void CHCSmtLib2Interface::declareVariable(string const& _name, SortPointer const& _sort)
{
	smtAssert(_sort, "");
	if (_sort->kind == Kind::Function)
		declareFunction(_name, _sort);
	else if (!m_variables.count(_name))
	{
		m_variables.insert(_name);
		write("(declare-var |" + _name + "| " + m_smtlib2->toSmtLibSort(*_sort) + ')');
	}
}

void CHCSmtLib2Interface::declareFunction(string const& _name, SortPointer const& _sort)
{
	smtAssert(_sort, "");
	smtAssert(_sort->kind == Kind::Function, "");
	// TODO Use domain and codomain as key as well
	if (!m_variables.count(_name))
	{
		auto fSort = dynamic_pointer_cast<FunctionSort>(_sort);
		smtAssert(fSort->codomain, "");
		string domain = m_smtlib2->toSmtLibSort(fSort->domain);
		string codomain = m_smtlib2->toSmtLibSort(*fSort->codomain);
		m_variables.insert(_name);
		write(
			"(declare-fun |" +
			_name +
			"| " +
			domain +
			" " +
			codomain +
			")"
		);
	}
}

void CHCSmtLib2Interface::write(string _data)
{
	m_accumulatedOutput += move(_data) + "\n";
}

string CHCSmtLib2Interface::querySolver(string const& _input)
{
	util::h256 inputHash = util::keccak256(_input);
	if (m_queryResponses.count(inputHash))
		return m_queryResponses.at(inputHash);
	if (m_smtCallback)
	{
		auto result = m_smtCallback(ReadCallback::kindString(ReadCallback::Kind::SMTQuery), _input);
		if (result.success)
			return result.responseOrErrorMessage;
	}
	m_unhandledQueries.push_back(_input);
	return "unknown\n";
}
