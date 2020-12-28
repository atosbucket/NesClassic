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

#include <libsolidity/formal/Predicate.h>

namespace solidity::frontend::smt
{

class EncodingContext;

/**
 * This file represents the specification for building CHC predicate instances.
 * The predicates follow the specification in PredicateSort.h.
 * */

smtutil::Expression interfacePre(Predicate const& _pred, ContractDefinition const& _contract, EncodingContext& _context);

smtutil::Expression interface(Predicate const& _pred, ContractDefinition const& _contract, EncodingContext& _context);

smtutil::Expression nondetInterface(Predicate const& _pred, ContractDefinition const& _contract, EncodingContext& _context, unsigned _preIdx, unsigned _postIdx);

smtutil::Expression constructor(Predicate const& _pred, EncodingContext& _context);
smtutil::Expression constructorCall(Predicate const& _pred, EncodingContext& _context);

smtutil::Expression function(
	Predicate const& _pred,
	ContractDefinition const* _contract,
	EncodingContext& _context
);

smtutil::Expression functionCall(
	Predicate const& _pred,
	ContractDefinition const* _contract,
	EncodingContext& _context
);

smtutil::Expression functionBlock(
	Predicate const& _pred,
	FunctionDefinition const& _function,
	ContractDefinition const* _contract,
	EncodingContext& _context
);

/// Helpers

std::vector<smtutil::Expression> initialStateVariables(ContractDefinition const& _contract, EncodingContext& _context);

std::vector<smtutil::Expression> stateVariablesAtIndex(unsigned _index, ContractDefinition const& _contract, EncodingContext& _context);

std::vector<smtutil::Expression> currentStateVariables(ContractDefinition const& _contract, EncodingContext& _context);

std::vector<smtutil::Expression> newStateVariables(ContractDefinition const& _contract, EncodingContext& _context);

std::vector<smtutil::Expression> currentFunctionVariablesForDefinition(
	FunctionDefinition const& _function,
	ContractDefinition const* _contract,
	EncodingContext& _context
);

std::vector<smtutil::Expression> currentFunctionVariablesForCall(
	FunctionDefinition const& _function,
	ContractDefinition const* _contract,
	EncodingContext& _context
);

std::vector<smtutil::Expression> currentBlockVariables(
	FunctionDefinition const& _function,
	ContractDefinition const* _contract,
	EncodingContext& _context
);

}
