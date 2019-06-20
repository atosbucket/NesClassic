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
 * Optimisation stage that replaces expressions of type ``sload(x)`` by the value
 * currently stored in storage, if known.
 */

#pragma once

#include <libyul/optimiser/DataFlowAnalyzer.h>

namespace yul
{

struct EVMDialect;

/**
 * Optimisation stage that replaces expressions of type ``sload(x)`` and ``mload(x)`` by the value
 * currently stored in storage resp. memory, if known.
 *
 * Works best if the code is in SSA form.
 *
 * Prerequisite: Disambiguator, ForLoopInitRewriter.
 */
class LoadResolver: public DataFlowAnalyzer
{
public:
	static void run(Dialect const& _dialect, Block& _ast);

private:
	LoadResolver(Dialect const& _dialect, bool _optimizeMLoad):
		DataFlowAnalyzer(_dialect),
		m_optimizeMLoad(_optimizeMLoad)
	{}

protected:
	using ASTModifier::visit;
	void visit(Expression& _e) override;

	bool m_optimizeMLoad = false;
};

}
