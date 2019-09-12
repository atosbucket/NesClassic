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

#include <fstream>

#include <test/tools/ossfuzz/yulProto.pb.h>
#include <test/tools/fuzzer_common.h>
#include <test/tools/ossfuzz/protoToYul.h>
#include <src/libfuzzer/libfuzzer_macro.h>

#include <libyul/AssemblyStack.h>
#include <libyul/backends/evm/EVMDialect.h>
#include <libyul/Exceptions.h>

#include <liblangutil/EVMVersion.h>
#include <liblangutil/SourceReferenceFormatter.h>

#include <test/tools/ossfuzz/yulFuzzerCommon.h>

using namespace yul;
using namespace yul::test::yul_fuzzer;
using namespace std;

using namespace langutil;
using namespace dev;
using namespace yul::test;

namespace
{
void printErrors(ostream& _stream, ErrorList const& _errors)
{
	SourceReferenceFormatter formatter(_stream);

	for (auto const& error: _errors)
		formatter.printExceptionInformation(
			*error,
			(error->type() == Error::Type::Warning) ? "Warning" : "Error"
		);
}
}

DEFINE_PROTO_FUZZER(Program const& _input)
{
	ProtoConverter converter;
	string yul_source = converter.programToString(_input);

	if (const char* dump_path = getenv("PROTO_FUZZER_DUMP_PATH"))
	{
		// With libFuzzer binary run this to generate a YUL source file x.yul:
		// PROTO_FUZZER_DUMP_PATH=x.yul ./a.out proto-input
		ofstream of(dump_path);
		of.write(yul_source.data(), yul_source.size());
	}

	if (yul_source.size() > 1200)
		return;

	YulStringRepository::reset();

	// AssemblyStack entry point
	AssemblyStack stack(
		langutil::EVMVersion(),
		AssemblyStack::Language::StrictAssembly,
		dev::solidity::OptimiserSettings::full()
	);

	try
	{
		// Parse protobuf mutated YUL code
		if (!stack.parseAndAnalyze("source", yul_source) || !stack.parserResult()->code ||
			!stack.parserResult()->analysisInfo)
		{
			printErrors(std::cout, stack.errors());
			return;
		}
	}
	catch (Exception const&)
	{
		return;
	}

	ostringstream os1;
	ostringstream os2;
	try
	{
		yulFuzzerUtil::interpret(
			os1,
			stack.parserResult()->code,
			EVMDialect::strictAssemblyForEVMObjects(langutil::EVMVersion())
		);
	}
	catch (yul::test::StepLimitReached const&)
	{
		return;
	}
	catch (yul::test::InterpreterTerminatedGeneric const&)
	{
	}

	stack.optimize();
	try
	{
		yulFuzzerUtil::interpret(
			os2,
			stack.parserResult()->code,
			EVMDialect::strictAssemblyForEVMObjects(langutil::EVMVersion()),
			(yul::test::yul_fuzzer::yulFuzzerUtil::maxSteps * 1.5)
		);
	}
	catch (yul::test::InterpreterTerminatedGeneric const&)
	{
	}

	bool isTraceEq = (os1.str() == os2.str());
	yulAssert(isTraceEq, "Interpreted traces for optimized and unoptimized code differ.");
	return;
}
