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
/** @file JSON.h
 * @date 2016
 *
 * JSON related helpers
 */

#pragma once

#include <json/json.h>

#include <string>

namespace solidity::util
{

/// Removes members with null value recursively from (@a _json).
Json::Value removeNullMembers(Json::Value _json);

/// Serialise the JSON object (@a _input) with indentation
std::string jsonPrettyPrint(Json::Value const& _input);

/// Serialise the JSON object (@a _input) without indentation
std::string jsonCompactPrint(Json::Value const& _input);

/// Parse a JSON string (@a _input) with enabled strict-mode and writes resulting JSON object to (@a _json)
/// \param _input JSON input string
/// \param _json [out] resulting JSON object
/// \param _errs [out] Formatted error messages
/// \return \c true if the document was successfully parsed, \c false if an error occurred.
bool jsonParseStrict(std::string const& _input, Json::Value& _json, std::string* _errs = nullptr);

}
