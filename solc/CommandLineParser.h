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
 * Validates and parses command-line options into an internal representation.
 */
#pragma once

#include <libsolidity/interface/CompilerStack.h>
#include <libsolidity/interface/DebugSettings.h>
#include <libsolidity/interface/FileReader.h>
#include <libsolidity/interface/ImportRemapper.h>

#include <libyul/AssemblyStack.h>

#include <liblangutil/DebugInfoSelection.h>
#include <liblangutil/EVMVersion.h>

#include <libsolutil/JSON.h>

#include <boost/program_options.hpp>
#include <boost/filesystem/path.hpp>

#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <set>
#include <string>
#include <vector>

namespace solidity::frontend
{

enum class InputMode
{
	Compiler,
	CompilerWithASTImport,
	StandardJson,
	Linker,
	Assembler,
};

struct CompilerOutputs
{
	bool operator!=(CompilerOutputs const& _other) const noexcept { return !(*this == _other); }
	bool operator==(CompilerOutputs const& _other) const noexcept;
	friend std::ostream& operator<<(std::ostream& _out, CompilerOutputs const& _requests);

	static std::string const& componentName(bool CompilerOutputs::* _component);
	static auto const& componentMap()
	{
		static std::map<std::string, bool CompilerOutputs::*> const components = {
			{"ast-compact-json", &CompilerOutputs::astCompactJson},
			{"asm", &CompilerOutputs::asm_},
			{"asm-json", &CompilerOutputs::asmJson},
			{"opcodes", &CompilerOutputs::opcodes},
			{"bin", &CompilerOutputs::binary},
			{"bin-runtime", &CompilerOutputs::binaryRuntime},
			{"abi", &CompilerOutputs::abi},
			{"ir", &CompilerOutputs::ir},
			{"ir-optimized", &CompilerOutputs::irOptimized},
			{"ewasm", &CompilerOutputs::ewasm},
			{"hashes", &CompilerOutputs::signatureHashes},
			{"userdoc", &CompilerOutputs::natspecUser},
			{"devdoc", &CompilerOutputs::natspecDev},
			{"metadata", &CompilerOutputs::metadata},
			{"storage-layout", &CompilerOutputs::storageLayout},
		};
		return components;
	}

	bool astCompactJson = false;
	bool asm_ = false;
	bool asmJson = false;
	bool opcodes = false;
	bool binary = false;
	bool binaryRuntime = false;
	bool abi = false;
	bool ir = false;
	bool irOptimized = false;
	bool ewasm = false;
	bool signatureHashes = false;
	bool natspecUser = false;
	bool natspecDev = false;
	bool metadata = false;
	bool storageLayout = false;
};

struct CombinedJsonRequests
{
	bool operator!=(CombinedJsonRequests const& _other) const noexcept { return !(*this == _other); }
	bool operator==(CombinedJsonRequests const& _other) const noexcept;
	friend std::ostream& operator<<(std::ostream& _out, CombinedJsonRequests const& _requests);

	static std::string const& componentName(bool CombinedJsonRequests::* _component);
	static auto const& componentMap()
	{
		static std::map<std::string, bool CombinedJsonRequests::*> const components = {
			{"abi", &CombinedJsonRequests::abi},
			{"metadata", &CombinedJsonRequests::metadata},
			{"bin", &CombinedJsonRequests::binary},
			{"bin-runtime", &CombinedJsonRequests::binaryRuntime},
			{"opcodes", &CombinedJsonRequests::opcodes},
			{"asm", &CombinedJsonRequests::asm_},
			{"storage-layout", &CombinedJsonRequests::storageLayout},
			{"generated-sources", &CombinedJsonRequests::generatedSources},
			{"generated-sources-runtime", &CombinedJsonRequests::generatedSourcesRuntime},
			{"srcmap", &CombinedJsonRequests::srcMap},
			{"srcmap-runtime", &CombinedJsonRequests::srcMapRuntime},
			{"function-debug", &CombinedJsonRequests::funDebug},
			{"function-debug-runtime", &CombinedJsonRequests::funDebugRuntime},
			{"hashes", &CombinedJsonRequests::signatureHashes},
			{"devdoc", &CombinedJsonRequests::natspecDev},
			{"userdoc", &CombinedJsonRequests::natspecUser},
			{"ast", &CombinedJsonRequests::ast},
		};
		return components;
	}

	bool abi = false;
	bool metadata = false;
	bool binary = false;
	bool binaryRuntime = false;
	bool opcodes = false;
	bool asm_ = false;
	bool storageLayout = false;
	bool generatedSources = false;
	bool generatedSourcesRuntime = false;
	bool srcMap = false;
	bool srcMapRuntime = false;
	bool funDebug = false;
	bool funDebugRuntime = false;
	bool signatureHashes = false;
	bool natspecDev = false;
	bool natspecUser = false;
	bool ast = false;
};

struct CommandLineOptions
{
	bool operator==(CommandLineOptions const& _other) const noexcept;
	bool operator!=(CommandLineOptions const& _other) const noexcept { return !(*this == _other); }

	OptimiserSettings optimiserSettings() const;

	struct
	{
		InputMode mode = InputMode::Compiler;
		std::set<boost::filesystem::path> paths;
		std::vector<ImportRemapper::Remapping> remappings;
		bool addStdin = false;
		boost::filesystem::path basePath = "";
		std::vector<boost::filesystem::path> includePaths;
		FileReader::FileSystemPathSet allowedDirectories;
		bool ignoreMissingFiles = false;
		bool errorRecovery = false;
	} input;

	struct
	{
		boost::filesystem::path dir;
		bool overwriteFiles = false;
		langutil::EVMVersion evmVersion;
		bool experimentalViaIR = false;
		RevertStrings revertStrings = RevertStrings::Default;
		std::optional<langutil::DebugInfoSelection> debugInfoSelection;
		CompilerStack::State stopAfter = CompilerStack::State::CompilationSuccessful;
	} output;

	struct
	{
		yul::AssemblyStack::Machine targetMachine = yul::AssemblyStack::Machine::EVM;
		yul::AssemblyStack::Language inputLanguage = yul::AssemblyStack::Language::StrictAssembly;
	} assembly;

	struct
	{
		std::map<std::string, util::h160> libraries; // library name -> address
	} linker;

	struct
	{
		util::JsonFormat json;
		std::optional<bool> coloredOutput;
		bool withErrorIds = false;
	} formatting;

	struct
	{
		CompilerOutputs outputs;
		bool estimateGas = false;
		std::optional<CombinedJsonRequests> combinedJsonRequests;
	} compiler;

	struct
	{
		CompilerStack::MetadataHash hash = CompilerStack::MetadataHash::IPFS;
		bool literalSources = false;
	} metadata;

	struct
	{
		bool enabled = false;
		std::optional<unsigned> expectedExecutionsPerDeployment;
		bool noOptimizeYul = false;
		std::optional<std::string> yulSteps;
	} optimizer;

	struct
	{
		bool initialize = false;
		ModelCheckerSettings settings;
	} modelChecker;
};

/// Parses the command-line arguments and produces a filled-out CommandLineOptions structure.
/// Validates provided values and prints error messages in case of errors.
///
/// The class is also responsible for handling options that only result in printing informational
/// text, without the need to invoke the compiler - printing usage banner, version or license.
class CommandLineParser
{
public:
	explicit CommandLineParser(std::ostream& _sout, std::ostream& _serr):
		m_sout(_sout),
		m_serr(_serr)
	{}

	/// Parses the command-line arguments and fills out the internal CommandLineOptions structure.
	/// Performs validation and prints error messages. If requested, prints usage banner, version
	/// or license.
	/// @param interactiveTerminal specifies whether the terminal is taking input from the user.
	/// This is used to determine whether to provide more user-friendly output in some situations.
	/// E.g. whether to print help text when no arguments are provided.
	/// @return true if there were no validation errors when parsing options and the
	/// CommandLineOptions structure has been fully initialized. false if there were errors - in
	/// this case CommandLineOptions may be only partially filled out. May also return false if
	/// there is not further processing necessary and the program should just exit.
	bool parse(int _argc, char const* const* _argv, bool _interactiveTerminal);

	CommandLineOptions const& options() const { return m_options; }

	/// Returns true if the parser has written anything to any of its output streams.
	bool hasOutput() const { return m_hasOutput; }

private:
	/// @returns a specification of all named command-line options accepted by the compiler.
	/// The object can be used to parse command-line arguments or to generate the help screen.
	static boost::program_options::options_description optionsDescription();

	/// @returns a specification of all positional command-line arguments accepted by the compiler.
	/// The object can be used to parse command-line arguments or to generate the help screen.
	static boost::program_options::positional_options_description positionalOptionsDescription();

	/// Uses boost::program_options to parse the command-line arguments and leaves the result in @a m_args.
	/// Also handles the arguments that result in information being printed followed by immediate exit.
	/// @returns false if parsing fails due to syntactical errors or the arguments not matching the description.
	bool parseArgs(int _argc, char const* const* _argv, bool _interactiveTerminal);

	/// Validates parsed arguments stored in @a m_args and fills out the internal CommandLineOptions
	/// structure.
	/// @return false if there are any validation errors, true otherwise.
	bool processArgs();

	/// Parses the value supplied to --combined-json.
	/// @return false if there are any validation errors, true otherwise.
	bool parseCombinedJsonOption();

	/// Parses the names of the input files, remappings for all modes except for Standard JSON.
	/// Does not check if files actually exist.
	/// @return false if there are any validation errors, true otherwise.
	bool parseInputPathsAndRemappings();

	/// Tries to read from the file @a _input or interprets @a _input literally if that fails.
	/// It then tries to parse the contents and appends to m_options.libraries.
	/// @return false if there are any validation errors, true otherwise.
	bool parseLibraryOption(std::string const& _input);

	bool parseOutputSelection();

	bool checkMutuallyExclusive(std::vector<std::string> const& _optionNames);
	[[noreturn]] void printVersionAndExit();
	[[noreturn]] void printLicenseAndExit();
	size_t countEnabledOptions(std::vector<std::string> const& _optionNames) const;
	static std::string joinOptionNames(std::vector<std::string> const& _optionNames, std::string _separator = ", ");

	/// Returns the stream that should receive normal output. Sets m_hasOutput to true if the
	/// stream has ever been used.
	std::ostream& sout();

	/// Returns the stream that should receive error output. Sets m_hasOutput to true if the
	/// stream has ever been used.
	std::ostream& serr();

	std::ostream& m_sout;
	std::ostream& m_serr;
	bool m_hasOutput = false;

	CommandLineOptions m_options;

	/// Map of command-line arguments produced by boost::program_options.
	/// Serves as input for filling out m_options.
	boost::program_options::variables_map m_args;
};

}
