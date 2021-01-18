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

#pragma once

#include <test/libsolidity/AnalysisFramework.h>
#include <test/TestCase.h>
#include <test/CommonSyntaxTest.h>
#include <liblangutil/Exceptions.h>
#include <libsolutil/AnsiColorized.h>

#include <iosfwd>
#include <string>
#include <vector>
#include <utility>

namespace solidity::frontend::test
{

using solidity::test::SyntaxTestError;

class SyntaxTest: public AnalysisFramework, public solidity::test::CommonSyntaxTest
{
public:
	static std::unique_ptr<TestCase> create(Config const& _config)
	{
		return std::make_unique<SyntaxTest>(_config.filename, _config.evmVersion, false);
	}
	static std::unique_ptr<TestCase> createErrorRecovery(Config const& _config)
	{
		return std::make_unique<SyntaxTest>(_config.filename, _config.evmVersion, true);
	}
	SyntaxTest(std::string const& _filename, langutil::EVMVersion _evmVersion, bool _parserErrorRecovery = false);

	TestResult run(std::ostream& _stream, std::string const& _linePrefix = "", bool _formatted = false) override;

protected:
	/// Returns @param _sourceCode prefixed with the version pragma and the SPDX license identifier.
	static std::string addPreamble(std::string const& _sourceCode);

	void setupCompiler();
	void parseAndAnalyze() override;
	virtual void filterObtainedErrors();

	bool m_optimiseYul = true;
	bool m_parserErrorRecovery = false;
};

}
