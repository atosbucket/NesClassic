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
/** @file Whiskers.h
 * @author Chris <chis@ethereum.org>
 * @date 2017
 *
 * Moustache-like templates.
 */

#pragma once

#include <libsolutil/Exceptions.h>

#include <string>
#include <map>
#include <vector>

namespace solidity::util
{

DEV_SIMPLE_EXCEPTION(WhiskersError);

/**
 * Moustache-like templates.
 *
 * Usage:
 *     std::vector<std::map<std::string, std::string>> listValues(2);
 *     listValues[0]["k"] = "key1";
 *     listValues[0]["v"] = "value1";
 *     listValues[1]["k"] = "key2";
 *     listValues[1]["v"] = "value2";
 *     auto s = Whiskers("<?c><p1><!c>y</c>\n<#list><k> -> <v>\n</list>")
 *         ("p1", "HEAD")
 *         ("c", true)
 *         ("list", listValues)
 *         .render();
 *
 * results in s == "HEAD\nkey1 -> value1\nkey2 -> value2\n"
 *
 * Note that lists cannot themselves contain lists - this would be a future feature.
 *
 * The elements are:
 *  - Regular parameter: <name>
 *    just replaced
 *  - Condition parameter: <?name>...<!name>...</name>, where "<!name>" is optional
 *    replaced (and recursively expanded) by the first part if the condition is true
 *    and by the second (or empty string if missing) if the condition is false
 *  - Conditional string parameter: <?+name>...<!+name>...</+name>
 *    Works similar to a conditional parameter where the checked condition is
 *    that the regular (string) parameter called "name" is non-empty.
 *  - List parameter: <#list>...</list>
 *    The part between the tags is repeated as often as values are provided
 *    in the mapping. Each list element can have its own parameter -> value mapping.
 */
class Whiskers
{
public:
	using StringMap = std::map<std::string, std::string>;
	using StringListMap = std::map<std::string, std::vector<StringMap>>;

	explicit Whiskers(std::string _template = "");

	/// Sets a single regular parameter, <paramName>.
	Whiskers& operator()(std::string _parameter, std::string _value);
	Whiskers& operator()(std::string _parameter, char const* _value) { return (*this)(_parameter, std::string{_value}); }
	/// Sets a condition parameter, <?paramName>...<!paramName>...</paramName>
	Whiskers& operator()(std::string _parameter, bool _value);
	/// Sets a list parameter, <#listName> </listName>.
	Whiskers& operator()(
		std::string _listParameter,
		std::vector<StringMap> _values
	);

	std::string render() const;

private:
	// Prevent implicit cast to bool
	Whiskers& operator()(std::string _parameter, long long);
	void checkParameterValid(std::string const& _parameter) const;
	void checkParameterUnknown(std::string const& _parameter) const;

	/// Checks whether the string stored in `m_template` contains all the tags specified.
	/// @param _parameter name of the parameter. This name is used to construct the tag(s).
	/// @param _prefixes a vector of strings, where each element is used to compose the tag
	///        like `"<" + element + _parameter + ">"`. Each element of _prefixes is used as a prefix of the tag name.
	void checkTemplateContainsTags(std::string const& _parameter, std::vector<std::string> const& _prefixes) const;

	static std::string replace(
		std::string const& _template,
		StringMap const& _parameters,
		std::map<std::string, bool> const& _conditions,
		StringListMap const& _listParameters = StringListMap()
	);

	static std::string paramRegex() { return "[a-zA-Z0-9_$-]+"; }

	/// Joins the two maps throwing an exception if two keys are equal.
	static StringMap joinMaps(StringMap const& _a, StringMap const& _b);

	std::string m_template;
	StringMap m_parameters;
	std::map<std::string, bool> m_conditions;
	StringListMap m_listParameters;
};

}
