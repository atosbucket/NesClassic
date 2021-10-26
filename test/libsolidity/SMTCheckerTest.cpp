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

#include <test/libsolidity/SMTCheckerTest.h>
#include <test/Common.h>

#include <range/v3/action/remove_if.hpp>

using namespace std;
using namespace solidity;
using namespace solidity::langutil;
using namespace solidity::frontend;
using namespace solidity::frontend::test;

SMTCheckerTest::SMTCheckerTest(string const& _filename): SyntaxTest(_filename, EVMVersion{})
{
	auto const& showUnproved = m_reader.stringSetting("SMTShowUnproved", "yes");
	if (showUnproved == "no")
		m_modelCheckerSettings.showUnproved = false;
	else if (showUnproved == "yes")
		m_modelCheckerSettings.showUnproved = true;
	else
		BOOST_THROW_EXCEPTION(runtime_error("Invalid SMT \"show unproved\" choice."));

	m_modelCheckerSettings.solvers = smtutil::SMTSolverChoice::None();
	auto const& choice = m_reader.stringSetting("SMTSolvers", "any");
	if (choice == "any")
		m_modelCheckerSettings.solvers = smtutil::SMTSolverChoice::All();
	else if (choice == "none")
		m_modelCheckerSettings.solvers = smtutil::SMTSolverChoice::None();
	else if (!m_modelCheckerSettings.solvers.setSolver(choice))
		BOOST_THROW_EXCEPTION(runtime_error("Invalid SMT solver choice."));

	m_modelCheckerSettings.solvers &= ModelChecker::availableSolvers();

	/// Underflow and Overflow are not enabled by default for Solidity >=0.8.7,
	/// so we explicitly enable all targets for the tests.
	m_modelCheckerSettings.targets = ModelCheckerTargets::All();

	auto engine = ModelCheckerEngine::fromString(m_reader.stringSetting("SMTEngine", "all"));
	if (engine)
		m_modelCheckerSettings.engine = *engine;
	else
		BOOST_THROW_EXCEPTION(runtime_error("Invalid SMT engine choice."));

	if (m_modelCheckerSettings.solvers.none() || m_modelCheckerSettings.engine.none())
		m_shouldRun = false;

	auto const& ignoreCex = m_reader.stringSetting("SMTIgnoreCex", "no");
	if (ignoreCex == "no")
		m_ignoreCex = false;
	else if (ignoreCex == "yes")
		m_ignoreCex = true;
	else
		BOOST_THROW_EXCEPTION(runtime_error("Invalid SMT counterexample choice."));

	auto const& ignoreInv = m_reader.stringSetting("SMTIgnoreInv", "no");
	if (ignoreInv == "no")
		m_modelCheckerSettings.invariants = ModelCheckerInvariants::All();
	else if (ignoreInv == "yes")
		m_modelCheckerSettings.invariants = ModelCheckerInvariants::None();
	else
		BOOST_THROW_EXCEPTION(runtime_error("Invalid SMT invariant choice."));

	auto const& ignoreOSSetting = m_reader.stringSetting("SMTIgnoreOS", "none");
	for (string const& os: ignoreOSSetting | ranges::views::split(',') | ranges::to<vector<string>>())
	{
#ifdef __APPLE__
		if (os == "macos")
			m_shouldRun = false;
#elif _WIN32
		if (os == "windows")
			m_shouldRun = false;
#elif __linux__
		if (os == "linux")
			m_shouldRun = false;
#endif
	}
}

TestCase::TestResult SMTCheckerTest::run(ostream& _stream, string const& _linePrefix, bool _formatted)
{
	setupCompiler();
	compiler().setModelCheckerSettings(m_modelCheckerSettings);
	parseAndAnalyze();
	filterObtainedErrors();

	return conclude(_stream, _linePrefix, _formatted);
}

void SMTCheckerTest::filterObtainedErrors()
{
	SyntaxTest::filterObtainedErrors();

	static auto removeCex = [](vector<SyntaxTestError>& errors) {
		for (auto& e: errors)
			if (
				auto cexPos = e.message.find("\\nCounterexample");
				cexPos != string::npos
			)
				e.message = e.message.substr(0, cexPos);
	};

	if (m_ignoreCex)
		removeCex(m_errorList);
}
