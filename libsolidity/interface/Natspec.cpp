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
 * @author Lefteris <lefteris@ethdev.com>
 * @date 2014
 * Takes the parsed AST and produces the Natspec documentation:
 * https://github.com/ethereum/wiki/wiki/Ethereum-Natural-Specification-Format
 *
 * Can generally deal with JSON files
 */

#include <libsolidity/interface/Natspec.h>

#include <libsolidity/ast/AST.h>
#include <boost/range/irange.hpp>

using namespace std;
using namespace solidity;
using namespace solidity::frontend;

Json::Value Natspec::userDocumentation(ContractDefinition const& _contractDef)
{
	Json::Value doc;
	Json::Value methods(Json::objectValue);

	auto constructorDefinition(_contractDef.constructor());
	if (constructorDefinition)
	{
		string value = extractDoc(constructorDefinition->annotation().docTags, "notice");
		if (!value.empty())
			// add the constructor, only if we have any documentation to add
			methods["constructor"] = Json::Value(value);
	}

	string notice = extractDoc(_contractDef.annotation().docTags, "notice");
	if (!notice.empty())
		doc["notice"] = Json::Value(notice);

	for (auto const& it: _contractDef.interfaceFunctions())
		if (it.second->hasDeclaration())
			if (auto const* f = dynamic_cast<FunctionDefinition const*>(&it.second->declaration()))
			{
				string value = extractDoc(f->annotation().docTags, "notice");
				if (!value.empty())
				{
					Json::Value user;
					// since @notice is the only user tag if missing function should not appear
					user["notice"] = Json::Value(value);
					methods[it.second->externalSignature()] = user;
				}
			}
	doc["methods"] = methods;

	return doc;
}

Json::Value Natspec::devDocumentation(ContractDefinition const& _contractDef)
{
	Json::Value doc;
	Json::Value methods(Json::objectValue);

	auto author = extractDoc(_contractDef.annotation().docTags, "author");
	if (!author.empty())
		doc["author"] = author;
	auto title = extractDoc(_contractDef.annotation().docTags, "title");
	if (!title.empty())
		doc["title"] = title;
	auto dev = extractDoc(_contractDef.annotation().docTags, "dev");
	if (!dev.empty())
		doc["details"] = Json::Value(dev);

	auto constructorDefinition(_contractDef.constructor());
	if (constructorDefinition)
	{
		Json::Value constructor(devDocumentation(constructorDefinition->annotation().docTags));
		if (!constructor.empty())
			// add the constructor, only if we have any documentation to add
			methods["constructor"] = constructor;
	}

	for (auto const& it: _contractDef.interfaceFunctions())
	{
		if (!it.second->hasDeclaration())
			continue;
		if (auto fun = dynamic_cast<FunctionDefinition const*>(&it.second->declaration()))
		{
			Json::Value method(devDocumentation(fun->annotation().docTags));
			if (!method.empty())
			{
				// add the function, only if we have any documentation to add
				Json::Value jsonReturn = extractReturnParameterDocs(fun->annotation().docTags, *fun);

				if (!jsonReturn.empty())
					method["returns"] = jsonReturn;

				methods[it.second->externalSignature()] = method;
			}
		}
	}

	doc["methods"] = methods;

	return doc;
}

Json::Value Natspec::extractReturnParameterDocs(std::multimap<std::string, DocTag> const& _tags, FunctionDefinition const& _functionDef)
{
	Json::Value jsonReturn{Json::objectValue};
	auto returnDocs = _tags.equal_range("return");

	if (!_functionDef.returnParameters().empty())
	{
		size_t n = 0;
		for (auto i = returnDocs.first; i != returnDocs.second; i++)
		{
			string paramName = _functionDef.returnParameters().at(n)->name();
			string content = i->second.content;

			if (paramName.empty())
				paramName = "_" + std::to_string(n);
			else
			{
				//check to make sure the first word of the doc str is the same as the return name
				auto nameEndPos = content.find_first_of(" \t");
				solAssert(content.substr(0, nameEndPos) == paramName, "No return param name given: " + paramName);
				content = content.substr(nameEndPos+1);
			}

			jsonReturn[paramName] = Json::Value(content);
			n++;
		}
	}

	return jsonReturn;
}

string Natspec::extractDoc(multimap<string, DocTag> const& _tags, string const& _name)
{
	string value;
	auto range = _tags.equal_range(_name);
	for (auto i = range.first; i != range.second; i++)
		value += i->second.content;
	return value;
}

Json::Value Natspec::devDocumentation(std::multimap<std::string, DocTag> const& _tags)
{
	Json::Value json(Json::objectValue);
	auto dev = extractDoc(_tags, "dev");
	if (!dev.empty())
		json["details"] = Json::Value(dev);

	auto author = extractDoc(_tags, "author");
	if (!author.empty())
		json["author"] = author;

	Json::Value params(Json::objectValue);
	auto paramRange = _tags.equal_range("param");
	for (auto i = paramRange.first; i != paramRange.second; ++i)
		params[i->second.paramName] = Json::Value(i->second.content);

	if (!params.empty())
		json["params"] = params;

	return json;
}
