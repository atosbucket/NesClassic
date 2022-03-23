#!/usr/bin/env python3
# pragma pylint: disable=too-many-lines
import argparse
import fnmatch
import json
import os
import subprocess
import sys
import traceback
import re

from typing import Any, List, Optional, Tuple, Union
from enum import Enum, auto

import colorama # Enables the use of SGR & CUP terminal VT sequences on Windows.
from deepdiff import DeepDiff

# {{{ JsonRpcProcess
class BadHeader(Exception):
    def __init__(self, msg: str):
        super().__init__("Bad header: " + msg)


class JsonRpcProcess:
    exe_path: str
    exe_args: List[str]
    process: subprocess.Popen
    trace_io: bool

    def __init__(self, exe_path: str, exe_args: List[str], trace_io: bool = True):
        self.exe_path = exe_path
        self.exe_args = exe_args
        self.trace_io = trace_io

    def __enter__(self):
        self.process = subprocess.Popen(
            [self.exe_path, *self.exe_args],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
        )
        return self

    def __exit__(self, exception_type, exception_value, traceback) -> None:
        self.process.kill()
        self.process.wait(timeout=2.0)

    def trace(self, topic: str, message: str) -> None:
        if self.trace_io:
            print(f"{SGR_TRACE}{topic}:{SGR_RESET} {message}")

    def receive_message(self) -> Union[None, dict]:
        # Note, we should make use of timeout to avoid infinite blocking if nothing is received.
        CONTENT_LENGTH_HEADER = "Content-Length: "
        CONTENT_TYPE_HEADER = "Content-Type: "
        if self.process.stdout is None:
            return None
        message_size = None
        while True:
            # read header
            line = self.process.stdout.readline()
            if line == '':
                # server quit
                return None
            line = line.decode("utf-8")
            if not line.endswith("\r\n"):
                raise BadHeader("missing newline")
            # remove the "\r\n"
            line = line[:-2]
            if line == '':
                break # done with the headers
            if line.startswith(CONTENT_LENGTH_HEADER):
                line = line[len(CONTENT_LENGTH_HEADER):]
                if not line.isdigit():
                    raise BadHeader("size is not int")
                message_size = int(line)
            elif line.startswith(CONTENT_TYPE_HEADER):
                # nothing todo with type for now.
                pass
            else:
                raise BadHeader("unknown header")
        if message_size is None:
            raise BadHeader("missing size")
        rpc_message = self.process.stdout.read(message_size).decode("utf-8")
        json_object = json.loads(rpc_message)
        self.trace('receive_message', json.dumps(json_object, indent=4, sort_keys=True))
        return json_object

    def send_message(self, method_name: str, params: Optional[dict]) -> None:
        if self.process.stdin is None:
            return
        message = {
            'jsonrpc': '2.0',
            'method': method_name,
            'params': params
        }
        json_string = json.dumps(obj=message)
        rpc_message = f"Content-Length: {len(json_string)}\r\n\r\n{json_string}"
        self.trace(f'send_message ({method_name})', json.dumps(message, indent=4, sort_keys=True))
        self.process.stdin.write(rpc_message.encode("utf-8"))
        self.process.stdin.flush()

    def call_method(self, method_name: str, params: Optional[dict]) -> Any:
        self.send_message(method_name, params)
        return self.receive_message()

    def send_notification(self, name: str, params: Optional[dict] = None) -> None:
        self.send_message(name, params)

# }}}

SGR_RESET = '\033[m'
SGR_TRACE = '\033[1;36m'
SGR_NOTICE = '\033[1;35m'
SGR_TEST_BEGIN = '\033[1;33m'
SGR_ASSERT_BEGIN = '\033[1;34m'
SGR_STATUS_OKAY = '\033[1;32m'
SGR_STATUS_FAIL = '\033[1;31m'

class ExpectationFailed(Exception):
    def __init__(self, actual, expected):
        self.actual = json.dumps(actual, sort_keys=True)
        self.expected = json.dumps(expected, sort_keys=True)
        diff = json.dumps(DeepDiff(actual, expected), indent=4)
        super().__init__(
            f"\n\tExpected {self.expected}\n\tbut got {self.actual}.\n\t{diff}"
        )


def create_cli_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Solidity LSP Test suite")
    parser.set_defaults(fail_fast=False)
    parser.add_argument(
        "-f, --fail-fast",
        dest="fail_fast",
        action="store_true",
        help="Terminates the running tests on first failure."
    )
    parser.set_defaults(trace_io=False)
    parser.add_argument(
        "-T, --trace-io",
        dest="trace_io",
        action="store_true",
        help="Be more verbose by also printing assertions."
    )
    parser.set_defaults(print_assertions=False)
    parser.add_argument(
        "-v, --print-assertions",
        dest="print_assertions",
        action="store_true",
        help="Be more verbose by also printing assertions."
    )
    parser.add_argument(
        "-t, --test-pattern",
        dest="test_pattern",
        type=str,
        default="*",
        help="Filters all available tests by matching against this test pattern (using globbing)",
        nargs="?"
    )
    parser.add_argument(
        "solc_path",
        type=str,
        default="solc",
        help="Path to solc binary to test against",
        nargs="?"
    )
    parser.add_argument(
        "project_root_dir",
        type=str,
        default=f"{os.path.dirname(os.path.realpath(__file__))}/..",
        help="Path to Solidity project's root directory (must be fully qualified).",
        nargs="?"
    )
    return parser

class Counter:
    total: int = 0
    passed: int = 0
    failed: int = 0


class Marker(Enum):
    SimpleRange = auto()
    MultilineRange = auto()


# Returns the given marker with the end extended by 'amount'
def extendEnd(marker, amount=1):
    marker["end"]["character"] += amount
    return marker


class SolidityLSPTestSuite: # {{{
    test_counter = Counter()
    assertion_counter = Counter()
    print_assertions: bool = False
    trace_io: bool = False
    fail_fast: bool = False
    test_pattern: str
    marker_regexes: {}

    def __init__(self):
        colorama.init()
        args = create_cli_parser().parse_args()
        self.solc_path = args.solc_path
        self.project_root_dir = os.path.realpath(args.project_root_dir) + "/test/libsolidity/lsp"
        self.project_root_uri = "file://" + self.project_root_dir
        self.print_assertions = args.print_assertions
        self.trace_io = args.trace_io
        self.test_pattern = args.test_pattern
        self.fail_fast = args.fail_fast
        self.marker_regexes = {
            Marker.SimpleRange: re.compile(R"(?P<range>[\^]+) (?P<tag>@\w+)"),
            Marker.MultilineRange: re.compile(R"\^(?P<delimiter>[()]) (?P<tag>@\w+)$")
        }

        print(f"{SGR_NOTICE}test pattern: {self.test_pattern}{SGR_RESET}")

    def main(self) -> int:
        """
        Runs all test cases.
        Returns 0 on success and the number of failing assertions (capped to 127) otherwise.
        """
        all_tests = sorted([
            str(name)[5:]
            for name in dir(SolidityLSPTestSuite)
            if callable(getattr(SolidityLSPTestSuite, name)) and name.startswith("test_")
        ])
        filtered_tests = fnmatch.filter(all_tests, self.test_pattern)
        for method_name in filtered_tests:
            test_fn = getattr(self, 'test_' + method_name)
            title: str = test_fn.__name__[5:]
            print(f"{SGR_TEST_BEGIN}Testing {title} ...{SGR_RESET}")
            try:
                with JsonRpcProcess(self.solc_path, ["--lsp"], trace_io=self.trace_io) as solc:
                    test_fn(solc)
                    self.test_counter.passed += 1
            except ExpectationFailed:
                self.test_counter.failed += 1
                print(traceback.format_exc())
                if self.fail_fast:
                    break
            except Exception as e: # pragma pylint: disable=broad-except
                self.test_counter.failed += 1
                print(f"Unhandled exception {e.__class__.__name__} caught: {e}")
                print(traceback.format_exc())
                if self.fail_fast:
                    break

        print(
            f"\n{SGR_NOTICE}Summary:{SGR_RESET}\n\n"
            f"  Test cases: {self.test_counter.passed} passed, {self.test_counter.failed} failed\n"
            f"  Assertions: {self.assertion_counter.passed} passed, {self.assertion_counter.failed} failed\n"
        )

        return min(max(self.test_counter.failed, self.assertion_counter.failed), 127)

    def setup_lsp(self, lsp: JsonRpcProcess, expose_project_root=True):
        """
        Prepares the solc LSP server by calling `initialize`,
        and `initialized` methods.
        """
        params = {
            'processId': None,
            'rootUri': self.project_root_uri,
            'trace': 'off',
            'initializationOptions': {},
            'capabilities': {
                'textDocument': {
                    'publishDiagnostics': {'relatedInformation': True}
                },
                'workspace': {
                    'applyEdit': True,
                    'configuration': True,
                    'didChangeConfiguration': {'dynamicRegistration': True},
                    'workspaceEdit': {'documentChanges': True},
                    'workspaceFolders': True
                }
            }
        }
        if not expose_project_root:
            params['rootUri'] = None
        lsp.call_method('initialize', params)
        lsp.send_notification('initialized')

    # {{{ helpers
    def get_test_file_path(self, test_case_name):
        return f"{self.project_root_dir}/{test_case_name}.sol"

    def get_test_file_uri(self, test_case_name):
        return "file://" + self.get_test_file_path(test_case_name)

    def get_test_file_contents(self, test_case_name):
        """
        Reads the file contents from disc for a given test case.
        The `test_case_name` will be the basename of the file
        in the test path (test/libsolidity/lsp).
        """
        with open(self.get_test_file_path(test_case_name), mode="r", encoding="utf-8", newline='') as f:
            return f.read()

    def require_params_for_method(self, method_name: str, message: dict) -> Any:
        """
        Ensures the given RPC message does contain the
        field 'method' with the given method name,
        and then returns its passed params.
        An exception is raised on expectation failures.
        """
        assert message is not None
        if 'error' in message.keys():
            code = message['error']["code"]
            text = message['error']['message']
            raise RuntimeError(f"Error {code} received. {text}")
        if 'method' not in message.keys():
            raise RuntimeError("No method received but something else.")
        self.expect_equal(message['method'], method_name, "Ensure expected method name")
        return message['params']

    def wait_for_diagnostics(self, solc: JsonRpcProcess, count: int) -> List[dict]:
        """
        Return `count` number of published diagnostic reports sorted by file URI.
        """
        reports = []
        for _ in range(0, count):
            message = solc.receive_message()
            assert message is not None # This can happen if the server aborts early.
            reports.append(
                self.require_params_for_method(
                    'textDocument/publishDiagnostics',
                    message,
                )
            )
        return sorted(reports, key=lambda x: x['uri'])

    def open_file_and_wait_for_diagnostics(
        self,
        solc_process: JsonRpcProcess,
        test_case_name: str,
        max_diagnostic_reports: int = 1
    ) -> List[Any]:
        """
        Opens file for given test case and waits for diagnostics to be published.
        """
        assert max_diagnostic_reports > 0
        solc_process.send_message(
            'textDocument/didOpen',
            {
                'textDocument':
                {
                    'uri': self.get_test_file_uri(test_case_name),
                    'languageId': 'Solidity',
                    'version': 1,
                    'text': self.get_test_file_contents(test_case_name)
                }
            }
        )
        return self.wait_for_diagnostics(solc_process, max_diagnostic_reports)

    def expect_equal(self, actual, expected, description="Equality") -> None:
        self.assertion_counter.total += 1
        prefix = f"[{self.assertion_counter.total}] {SGR_ASSERT_BEGIN}{description}: "
        diff = DeepDiff(actual, expected)
        if len(diff) == 0:
            self.assertion_counter.passed += 1
            if self.print_assertions:
                print(prefix + SGR_STATUS_OKAY + 'OK' + SGR_RESET)
            return

        # Failed assertions are always printed.
        self.assertion_counter.failed += 1
        print(prefix + SGR_STATUS_FAIL + 'FAILED' + SGR_RESET)
        raise ExpectationFailed(actual, expected)

    def expect_empty_diagnostics(self, published_diagnostics: List[dict]) -> None:
        self.expect_equal(len(published_diagnostics), 1, "one publish diagnostics notification")
        self.expect_equal(len(published_diagnostics[0]['diagnostics']), 0, "should not contain diagnostics")

    def expect_diagnostic(
        self,
        diagnostic,
        code: int,
        lineNo: int = None,
        startEndColumns: Tuple[int, int] = None,
        marker: {} = None
    ):
        self.expect_equal(diagnostic['code'], code, f'diagnostic: {code}')

        if marker:
            self.expect_equal(diagnostic['range'], marker, "diagnostic: check range")
        else:
            assert len(startEndColumns) == 2
            [startColumn, endColumn] = startEndColumns
            self.expect_equal(
                diagnostic['range'],
                {
                    'start': {'character': startColumn, 'line': lineNo},
                    'end': {'character': endColumn, 'line': lineNo}
                },
                "diagnostic: check range"
            )


    def expect_location(
        self,
        obj: dict,
        uri: str,
        lineNo: int,
        startEndColumns: Tuple[int, int]
    ):
        """
        obj is an JSON object containing two keys:
            - 'uri': a string of the document URI
            - 'range': the location range, two child objects 'start' and 'end',
                        each having a 'line' and 'character' integer value.
        """
        [startColumn, endColumn] = startEndColumns
        self.expect_equal(obj['uri'], uri)
        self.expect_equal(obj['range']['start']['line'], lineNo)
        self.expect_equal(obj['range']['start']['character'], startColumn)
        self.expect_equal(obj['range']['end']['line'], lineNo)
        self.expect_equal(obj['range']['end']['character'], endColumn)

    def expect_goto_definition_location(
        self,
        solc: JsonRpcProcess,
        document_uri: str,
        document_position: Tuple[int, int],
        expected_uri: str,
        expected_lineNo: int,
        expected_startEndColumns: Tuple[int, int],
        description: str
    ):
        response = solc.call_method(
            'textDocument/definition',
            {
                'textDocument': {
                    'uri': document_uri,
                },
                'position': {
                    'line': document_position[0],
                    'character': document_position[1]
                }
            }
        )
        message = "Goto definition (" + description + ")"
        self.expect_equal(len(response['result']), 1, message)
        self.expect_location(response['result'][0], expected_uri, expected_lineNo, expected_startEndColumns)
    # }}}

    # {{{ actual tests
    def test_publish_diagnostics_warnings(self, solc: JsonRpcProcess) -> None:
        self.setup_lsp(solc)
        TEST_NAME = 'publish_diagnostics_1'
        published_diagnostics = self.open_file_and_wait_for_diagnostics(solc, TEST_NAME)

        self.expect_equal(len(published_diagnostics), 1, "One published_diagnostics message")
        report = published_diagnostics[0]

        self.expect_equal(report['uri'], self.get_test_file_uri(TEST_NAME), "Correct file URI")
        diagnostics = report['diagnostics']

        markers = self.get_file_tags(TEST_NAME)

        self.expect_equal(len(diagnostics), 3, "3 diagnostic messages")
        self.expect_diagnostic(diagnostics[0], code=6321, marker=markers["@unusedReturnVariable"])
        self.expect_diagnostic(diagnostics[1], code=2072, marker=markers["@unusedVariable"])
        self.expect_diagnostic(diagnostics[2], code=2072, marker=markers["@unusedContractVariable"])

    def test_publish_diagnostics_errors(self, solc: JsonRpcProcess) -> None:
        self.setup_lsp(solc)
        TEST_NAME = 'publish_diagnostics_2'
        published_diagnostics = self.open_file_and_wait_for_diagnostics(solc, TEST_NAME)

        self.expect_equal(len(published_diagnostics), 1, "One published_diagnostics message")
        report = published_diagnostics[0]

        self.expect_equal(report['uri'], self.get_test_file_uri(TEST_NAME), "Correct file URI")
        diagnostics = report['diagnostics']

        markers = self.get_file_tags(TEST_NAME)

        self.expect_equal(len(diagnostics), 3, "3 diagnostic messages")
        self.expect_diagnostic(diagnostics[0], code=9574, marker=markers["@conversionError"])
        self.expect_diagnostic(diagnostics[1], code=6777, marker=markers["@argumentsRequired"])
        self.expect_diagnostic(diagnostics[2], code=6160, marker=markers["@wrongArgumentsCount"])

    def test_publish_diagnostics_errors_multiline(self, solc: JsonRpcProcess) -> None:
        self.setup_lsp(solc)
        TEST_NAME = 'publish_diagnostics_3'
        published_diagnostics = self.open_file_and_wait_for_diagnostics(solc, TEST_NAME)

        self.expect_equal(len(published_diagnostics), 1, "One published_diagnostics message")
        report = published_diagnostics[0]

        self.expect_equal(report['uri'], self.get_test_file_uri(TEST_NAME), "Correct file URI")
        diagnostics = report['diagnostics']

        self.expect_equal(len(diagnostics), 1, "1 diagnostic messages")
        self.expect_equal(diagnostics[0]['code'], 3656, "diagnostic: check code")
        self.expect_equal(
            diagnostics[0]['range'],
            {
                'start': {'character': 0, 'line': 7},
                'end': {'character': 1, 'line': 10}
            },
            "diagnostic: check range"
        )

    def test_textDocument_didOpen_with_relative_import(self, solc: JsonRpcProcess) -> None:
        self.setup_lsp(solc)
        TEST_NAME = 'didOpen_with_import'
        published_diagnostics = self.open_file_and_wait_for_diagnostics(solc, TEST_NAME, 2)

        self.expect_equal(len(published_diagnostics), 2, "Diagnostic reports for 2 files")

        # primary file:
        report = published_diagnostics[0]
        self.expect_equal(report['uri'], self.get_test_file_uri(TEST_NAME), "Correct file URI")
        self.expect_equal(len(report['diagnostics']), 0, "no diagnostics")

        # imported file (./lib.sol):
        report = published_diagnostics[1]
        self.expect_equal(report['uri'], self.get_test_file_uri('lib'), "Correct file URI")
        self.expect_equal(len(report['diagnostics']), 1, "one diagnostic")
        marker = self.get_file_tags("lib")["@diagnostics"]
        self.expect_diagnostic(report['diagnostics'][0], code=2072, marker=marker)


    def get_file_tags(self, test_name: str, verbose=False):
        """
        Finds all tags (e.g. @tagname) in the given test and returns them as a
        dictionary having the following structure: {
            "@tagname": {
             "start": { "character": 3, "line": 2 },
             "end":   { "character": 30, "line": 2 }
            }
        }
        """
        content = self.get_test_file_contents(test_name)

        markers = {}

        for lineNum, line in enumerate(content.splitlines(), start=-1):
            commentStart = line.find("//")
            if commentStart == -1:
                continue

            for kind, regex in self.marker_regexes.items():
                for match in regex.finditer(line[commentStart:]):
                    if kind == Marker.SimpleRange:
                        markers[match.group("tag")] = {
                            "start": {
                                "line": lineNum,
                                "character": match.start("range") + commentStart
                            },
                            "end": {
                                "line": lineNum,
                                "character": match.end("range") + commentStart
                        }}
                    elif kind == Marker.MultilineRange:
                        if match.group("delimiter") == "(":
                            markers[match.group("tag")] = \
                                { "start": { "line": lineNum, "character": 0 } }
                        elif match.group("delimiter") == ")":
                            markers[match.group("tag")]["end"] = \
                                { "line": lineNum, "character": 0 }

        if verbose:
            print(markers)
        return markers


    def test_didChange_in_A_causing_error_in_B(self, solc: JsonRpcProcess) -> None:
        # Reusing another test but now change some file that generates an error in the other.
        self.test_textDocument_didOpen_with_relative_import(solc)
        marker = self.get_file_tags("lib")["@addFunction"]
        self.open_file_and_wait_for_diagnostics(solc, 'lib', 2)
        solc.send_message(
            'textDocument/didChange',
            {
                'textDocument':
                {
                    'uri': self.get_test_file_uri('lib')
                },
                'contentChanges':
                [
                    {
                        'range': marker,
                        'text': "" # deleting function `add`
                    }
                ]
            }
        )
        published_diagnostics = self.wait_for_diagnostics(solc, 2)
        self.expect_equal(len(published_diagnostics), 2, "Diagnostic reports for 2 files")

        # Main file now contains a new diagnostic
        report = published_diagnostics[0]
        self.expect_equal(report['uri'], self.get_test_file_uri('didOpen_with_import'))
        diagnostics = report['diagnostics']
        marker = self.get_file_tags("didOpen_with_import")["@diagnostics"]
        self.expect_equal(len(diagnostics), 1, "now, no diagnostics")
        self.expect_diagnostic(diagnostics[0], code=9582, marker=marker)

        # The modified file retains the same diagnostics.
        report = published_diagnostics[1]
        self.expect_equal(report['uri'], self.get_test_file_uri('lib'))
        self.expect_equal(len(report['diagnostics']), 0)
        # The warning went away because the compiler aborts further processing after the error.

    def test_textDocument_didOpen_with_relative_import_without_project_url(self, solc: JsonRpcProcess) -> None:
        self.setup_lsp(solc, expose_project_root=False)
        TEST_NAME = 'didOpen_with_import'
        published_diagnostics = self.open_file_and_wait_for_diagnostics(solc, TEST_NAME, 2)
        self.verify_didOpen_with_import_diagnostics(published_diagnostics)

    def verify_didOpen_with_import_diagnostics(
        self,
        published_diagnostics: List[Any],
        main_file_name='didOpen_with_import'
    ):
        self.expect_equal(len(published_diagnostics), 2, "Diagnostic reports for 2 files")

        # primary file:
        report = published_diagnostics[0]
        self.expect_equal(report['uri'], self.get_test_file_uri(main_file_name), "Correct file URI")
        self.expect_equal(len(report['diagnostics']), 0, "one diagnostic")

        # imported file (./lib.sol):
        report = published_diagnostics[1]
        self.expect_equal(report['uri'], self.get_test_file_uri('lib'), "Correct file URI")
        self.expect_equal(len(report['diagnostics']), 1, "one diagnostic")

        marker = self.get_file_tags('lib')["@diagnostics"]
        self.expect_diagnostic(report['diagnostics'][0], code=2072, marker=marker)


    def test_textDocument_didChange_updates_diagnostics(self, solc: JsonRpcProcess) -> None:
        self.setup_lsp(solc)
        TEST_NAME = 'publish_diagnostics_1'
        published_diagnostics = self.open_file_and_wait_for_diagnostics(solc, TEST_NAME)
        self.expect_equal(len(published_diagnostics), 1, "One published_diagnostics message")
        report = published_diagnostics[0]
        self.expect_equal(report['uri'], self.get_test_file_uri(TEST_NAME), "Correct file URI")
        diagnostics = report['diagnostics']
        self.expect_equal(len(diagnostics), 3, "3 diagnostic messages")
        markers = self.get_file_tags(TEST_NAME)
        self.expect_diagnostic(diagnostics[0], code=6321, marker=markers["@unusedReturnVariable"])
        self.expect_diagnostic(diagnostics[1], code=2072, marker=markers["@unusedVariable"])
        self.expect_diagnostic(diagnostics[2], code=2072, marker=markers["@unusedContractVariable"])

        solc.send_message(
            'textDocument/didChange',
            {
                'textDocument': {
                    'uri': self.get_test_file_uri(TEST_NAME)
                },
                'contentChanges': [
                    {
                        'range': extendEnd(markers["@unusedVariable"]),
                        'text': ""
                    }
                ]
            }
        )
        published_diagnostics = self.wait_for_diagnostics(solc, 1)
        self.expect_equal(len(published_diagnostics), 1)
        report = published_diagnostics[0]
        self.expect_equal(report['uri'], self.get_test_file_uri(TEST_NAME), "Correct file URI")
        diagnostics = report['diagnostics']
        self.expect_equal(len(diagnostics), 2)
        self.expect_diagnostic(diagnostics[0], code=6321, marker=markers["@unusedReturnVariable"])
        self.expect_diagnostic(diagnostics[1], code=2072, marker=markers["@unusedContractVariable"])

    def test_textDocument_didChange_delete_line_and_close(self, solc: JsonRpcProcess) -> None:
        # Reuse this test to prepare and ensure it is as expected
        self.test_textDocument_didOpen_with_relative_import(solc)
        self.open_file_and_wait_for_diagnostics(solc, 'lib', 2)

        marker = self.get_file_tags('lib')["@diagnostics"]

        # lib.sol: Fix the unused variable message by removing it.
        solc.send_message(
            'textDocument/didChange',
            {
                'textDocument':
                {
                    'uri': self.get_test_file_uri('lib')
                },
                'contentChanges': # delete the in-body statement: `uint unused;`
                [
                    {
                        'range': extendEnd(marker),
                        'text': ""
                    }
                ]
            }
        )
        published_diagnostics = self.wait_for_diagnostics(solc, 2)
        self.expect_equal(len(published_diagnostics), 2, "published diagnostics count")
        report1 = published_diagnostics[0]
        self.expect_equal(report1['uri'], self.get_test_file_uri('didOpen_with_import'), "Correct file URI")
        self.expect_equal(len(report1['diagnostics']), 0, "no diagnostics in didOpen_with_import.sol")
        report2 = published_diagnostics[1]
        self.expect_equal(report2['uri'], self.get_test_file_uri('lib'), "Correct file URI")
        self.expect_equal(len(report2['diagnostics']), 0, "no diagnostics in lib.sol")

        # Now close the file and expect the warning to re-appear
        solc.send_message(
            'textDocument/didClose',
            { 'textDocument': { 'uri': self.get_test_file_uri('lib') }}
        )

        published_diagnostics = self.wait_for_diagnostics(solc, 2)
        self.verify_didOpen_with_import_diagnostics(published_diagnostics)

    def test_textDocument_opening_two_new_files_edit_and_close(self, solc: JsonRpcProcess) -> None:
        """
        Open two new files A and B, let A import B, expect no error,
        then close B and now expect the error of file B not being found.
        """

        self.setup_lsp(solc)
        FILE_A_URI = 'file:///a.sol'
        solc.send_message('textDocument/didOpen', {
            'textDocument': {
                'uri': FILE_A_URI,
                'languageId': 'Solidity',
                'version': 1,
                'text': ''.join([
                    '// SPDX-License-Identifier: UNLICENSED\n',
                    'pragma solidity >=0.8.0;\n',
                ])
            }
        })
        reports = self.wait_for_diagnostics(solc, 1)
        self.expect_equal(len(reports), 1, "one publish diagnostics notification")
        self.expect_equal(len(reports[0]['diagnostics']), 0, "should not contain diagnostics")

        FILE_B_URI = 'file:///b.sol'
        solc.send_message('textDocument/didOpen', {
            'textDocument': {
                'uri': FILE_B_URI,
                'languageId': 'Solidity',
                'version': 1,
                'text': ''.join([
                    '// SPDX-License-Identifier: UNLICENSED\n',
                    'pragma solidity >=0.8.0;\n',
                ])
            }
        })
        reports = self.wait_for_diagnostics(solc, 2)
        self.expect_equal(len(reports), 2, "one publish diagnostics notification")
        self.expect_equal(len(reports[0]['diagnostics']), 0, "should not contain diagnostics")
        self.expect_equal(len(reports[1]['diagnostics']), 0, "should not contain diagnostics")

        solc.send_message('textDocument/didChange', {
            'textDocument': {
                'uri': FILE_A_URI
            },
            'contentChanges': [
                {
                    'range': {
                        'start': { 'line': 2, 'character': 0 },
                        'end': { 'line': 2, 'character': 0 }
                    },
                    'text': 'import "./b.sol";\n'
                }
            ]
        })
        reports = self.wait_for_diagnostics(solc, 2)
        self.expect_equal(len(reports), 2, "one publish diagnostics notification")
        self.expect_equal(len(reports[0]['diagnostics']), 0, "should not contain diagnostics")
        self.expect_equal(len(reports[1]['diagnostics']), 0, "should not contain diagnostics")

        solc.send_message(
            'textDocument/didClose',
            { 'textDocument': { 'uri': FILE_B_URI }}
        )
        # We only get one diagnostics message since the diagnostics for b.sol was empty.
        reports = self.wait_for_diagnostics(solc, 1)
        self.expect_equal(len(reports), 1, "one publish diagnostics notification")
        self.expect_diagnostic(reports[0]['diagnostics'][0], 6275, 2, (0, 17)) # a.sol: File B not found
        self.expect_equal(reports[0]['uri'], FILE_A_URI, "Correct uri")

    def test_textDocument_closing_virtual_file_removes_imported_real_file(self, solc: JsonRpcProcess) -> None:
        """
        We open a virtual file that imports a real file with a warning.
        Once we close the virtual file, the warning is removed from the diagnostics,
        since the real file is not considered part of the project anymore.
        """

        self.setup_lsp(solc)
        FILE_A_URI = f'file://{self.project_root_dir}/a.sol'
        solc.send_message('textDocument/didOpen', {
            'textDocument': {
                'uri': FILE_A_URI,
                'languageId': 'Solidity',
                'version': 1,
                'text':
                    '// SPDX-License-Identifier: UNLICENSED\n'
                    'pragma solidity >=0.8.0;\n'
                    'import "./lib.sol";\n'
            }
        })
        reports = self.wait_for_diagnostics(solc, 2)
        self.expect_equal(len(reports), 2, '')
        self.expect_equal(len(reports[0]['diagnostics']), 0, "should not contain diagnostics")

        marker = self.get_file_tags("lib")["@diagnostics"]

        # unused variable in lib.sol
        self.expect_diagnostic(reports[1]['diagnostics'][0], code=2072, marker=marker)

        # Now close the file and expect the warning for lib.sol to be removed
        solc.send_message(
            'textDocument/didClose',
            { 'textDocument': { 'uri': FILE_A_URI }}
        )
        reports = self.wait_for_diagnostics(solc, 1)
        self.expect_equal(len(reports), 1, '')
        self.expect_equal(reports[0]['uri'], f'file://{self.project_root_dir}/lib.sol', "")
        self.expect_equal(len(reports[0]['diagnostics']), 0, "should not contain diagnostics")


    def test_textDocument_didChange_at_eol(self, solc: JsonRpcProcess) -> None:
        """
        Append at one line and insert a new one below.
        """
        self.setup_lsp(solc)
        FILE_NAME = 'didChange_template'
        FILE_URI = self.get_test_file_uri(FILE_NAME)
        solc.send_message('textDocument/didOpen', {
            'textDocument': {
                'uri': FILE_URI,
                'languageId': 'Solidity',
                'version': 1,
                'text': self.get_test_file_contents(FILE_NAME)
            }
        })
        published_diagnostics = self.wait_for_diagnostics(solc, 1)
        self.expect_equal(len(published_diagnostics), 1, "one publish diagnostics notification")
        self.expect_equal(len(published_diagnostics[0]['diagnostics']), 0, "no diagnostics")
        solc.send_message('textDocument/didChange', {
            'textDocument': {
                'uri': FILE_URI
            },
            'contentChanges': [
                {
                    'range': {
                        'start': { 'line': 6, 'character': 0 },
                        'end': { 'line': 6, 'character': 0 }
                    },
                    'text': " f"
                }
            ]
        })
        published_diagnostics = self.wait_for_diagnostics(solc, 1)
        self.expect_equal(len(published_diagnostics), 1, "one publish diagnostics notification")
        report2 = published_diagnostics[0]
        self.expect_equal(report2['uri'], FILE_URI, "Correct file URI")
        self.expect_equal(len(report2['diagnostics']), 1, "one diagnostic")
        self.expect_diagnostic(report2['diagnostics'][0], 7858, 6, (1, 2))

        solc.send_message('textDocument/didChange', {
            'textDocument': { 'uri': FILE_URI },
            'contentChanges': [
                {
                    'range': {
                        'start': { 'line': 6, 'character': 2 },
                        'end': { 'line': 6, 'character': 2 }
                    },
                    'text': 'unction f() public {}'
                }
            ]
        })
        published_diagnostics = self.wait_for_diagnostics(solc, 1)
        self.expect_equal(len(published_diagnostics), 1, "one publish diagnostics notification")
        report3 = published_diagnostics[0]
        self.expect_equal(report3['uri'], FILE_URI, "Correct file URI")
        self.expect_equal(len(report3['diagnostics']), 1, "one diagnostic")
        self.expect_diagnostic(report3['diagnostics'][0], 4126, 6, (1, 23))

    def test_textDocument_definition(self, solc: JsonRpcProcess) -> None:
        self.setup_lsp(solc)
        FILE_NAME = 'goto_definition'
        FILE_URI = self.get_test_file_uri(FILE_NAME)
        LIB_URI = self.get_test_file_uri('lib')
        solc.send_message('textDocument/didOpen', {
            'textDocument': {
                'uri': FILE_URI,
                'languageId': 'Solidity',
                'version': 1,
                'text': self.get_test_file_contents(FILE_NAME)
            }
        })
        published_diagnostics = self.wait_for_diagnostics(solc, 2)
        self.expect_equal(len(published_diagnostics), 2, "publish diagnostics for 2 files")
        self.expect_equal(len(published_diagnostics[0]['diagnostics']), 0)
        self.expect_equal(len(published_diagnostics[1]['diagnostics']), 1)
        self.expect_diagnostic(published_diagnostics[1]['diagnostics'][0], 2072, 33, (8, 19)) # unused variable in lib.sol

        # import directive
        self.expect_goto_definition_location(
            solc=solc,
            document_uri=FILE_URI,
            document_position=(3, 9), # symbol `"./lib.sol"` in `import "./lib.sol"`
            expected_uri=LIB_URI,
            expected_lineNo=0,
            expected_startEndColumns=(0, 0),
            description="import directive"
        )

        # type symbol to jump to type defs (error, contract, enum, ...)
        self.expect_goto_definition_location(
            solc=solc,
            document_uri=FILE_URI,
            document_position=(30, 19), # symbol `IA` in `new IA()`
            expected_uri=FILE_URI,
            expected_lineNo=10,
            expected_startEndColumns=(9, 11),
            description="type symbol to jump to definition"
        )

        # virtual function lookup?
        self.expect_goto_definition_location(
            solc=solc,
            document_uri=FILE_URI,
            document_position=(31, 12), # symbol `f`, jumps to interface definition
            expected_uri=FILE_URI,
            expected_lineNo=7,
            expected_startEndColumns=(13, 14),
            description="virtual function lookup"
        )

        # using for
        self.expect_goto_definition_location(
            solc=solc,
            document_uri=FILE_URI,
            document_position=(37, 10), # symbol `add` in `i.add(5)`
            expected_uri=FILE_URI,
            expected_lineNo=22,
            expected_startEndColumns=(13, 16),
            description="using for"
        )

        # library
        self.expect_goto_definition_location(
            solc=solc,
            document_uri=FILE_URI,
            document_position=(43, 15), # symbol `Lib` in `Lib.add(n, 1)`
            expected_uri=LIB_URI,
            expected_lineNo=22,
            expected_startEndColumns=(8, 11),
            description="Library symbol from different file"
        )
        self.expect_goto_definition_location(
            solc=solc,
            document_uri=FILE_URI,
            document_position=(43, 19), # symbol `add` in `Lib.add(n, 1)`
            expected_uri=LIB_URI,
            expected_lineNo=24,
            expected_startEndColumns=(13, 16),
            description="Library member symbol from different file"
        )

        # enum type symbol and enum values
        self.expect_goto_definition_location(
            solc=solc,
            document_uri=FILE_URI,
            document_position=(46, 19), # symbol `Color` in function signature's parameter
            expected_uri=LIB_URI,
            expected_lineNo=13,
            expected_startEndColumns=(5, 10),
            description="Enum type"
        )
        self.expect_goto_definition_location(
            solc=solc,
            document_uri=FILE_URI,
            document_position=(48, 24), # symbol `Red` in `Color.Red`
            expected_uri=LIB_URI,
            expected_lineNo=15,
            expected_startEndColumns=(4, 7),
            description="Enum value"
        )
        self.expect_goto_definition_location(
            solc=solc,
            document_uri=FILE_URI,
            document_position=(48, 24), # symbol `Red` in `Color.Red`
            expected_uri=LIB_URI,
            expected_lineNo=15,
            expected_startEndColumns=(4, 7),
            description="Enum value"
        )

        # local variable declarations
        self.expect_goto_definition_location(
            solc=solc,
            document_uri=FILE_URI,
            document_position=(49, 17), # symbol `e` in `(c == e)`
            expected_uri=FILE_URI,
            expected_lineNo=48,
            expected_startEndColumns=(14, 15),
            description="local variable declaration"
        )

        # User defined type
        self.expect_goto_definition_location(
            solc=solc,
            document_uri=FILE_URI,
            document_position=(58, 8), # symbol `Price` in `Price p ...`
            expected_uri=FILE_URI,
            expected_lineNo=55,
            expected_startEndColumns=(9, 14),
            description="User defined type on left hand side"
        )

        self.expect_goto_definition_location(
            solc=solc,
            document_uri=FILE_URI,
            document_position=(58, 18), # symbol `Price` in `Price.wrap()` expected_uri=FILE_URI,
            expected_uri=FILE_URI,
            expected_lineNo=55,
            expected_startEndColumns=(9, 14),
            description="User defined type on right hand side."
        )

        # struct constructor also properly jumps to the struct's declaration.
        self.expect_goto_definition_location(
            solc=solc,
            document_uri=FILE_URI,
            document_position=(64, 33), # symbol `RGBColor` right hand side expression.
            expected_uri=LIB_URI,
            expected_lineNo=38,
            expected_startEndColumns=(7, 15),
            description="Struct constructor."
        )

    def test_textDocument_definition_imports(self, solc: JsonRpcProcess) -> None:
        self.setup_lsp(solc)
        FILE_NAME = 'goto_definition_imports'
        FILE_URI = self.get_test_file_uri(FILE_NAME)
        LIB_URI = self.get_test_file_uri('lib')
        solc.send_message('textDocument/didOpen', {
            'textDocument': {
                'uri': FILE_URI,
                'languageId': 'Solidity',
                'version': 1,
                'text': self.get_test_file_contents(FILE_NAME)
            }
        })
        published_diagnostics = self.wait_for_diagnostics(solc, 2)
        self.expect_equal(len(published_diagnostics), 2, "publish diagnostics for 2 files")
        self.expect_equal(len(published_diagnostics[0]['diagnostics']), 0)
        self.expect_equal(len(published_diagnostics[1]['diagnostics']), 1)
        self.expect_diagnostic(published_diagnostics[1]['diagnostics'][0], 2072, 33, (8, 19)) # unused variable in lib.sol

        # import directive: test symbol alias
        self.expect_goto_definition_location(
            solc=solc,
            document_uri=FILE_URI,
            document_position=(3, 9), #  in `Weather` of `import {Weather as Wetter} from "./lib.sol"`
            expected_uri=LIB_URI,
            expected_lineNo=6,
            expected_startEndColumns=(5, 12),
            description="goto definition of symbol in symbol alias import directive"
        )

        # import directive: test symbol alias
        self.expect_goto_definition_location(
            solc=solc,
            document_uri=FILE_URI,
            document_position=(8, 55), # `Wetter` in return type declaration
            expected_uri=LIB_URI,
            expected_lineNo=6,
            expected_startEndColumns=(5, 12),
            description="goto definition of symbol in symbol alias import directive"
        )

        # That.Color tests with `That` being the aliased library to be imported.
        self.expect_goto_definition_location(
            solc=solc,
            document_uri=FILE_URI,
            document_position=(13, 55), # `That` in return type declaration
            expected_uri=LIB_URI,
            expected_lineNo=13,
            expected_startEndColumns=(5, 10),
            description="goto definition of symbol in symbol alias import directive"
        )
        self.expect_goto_definition_location(
            solc=solc,
            document_uri=FILE_URI,
            document_position=(15, 8),
            expected_uri=LIB_URI,
            expected_lineNo=13,
            expected_startEndColumns=(5, 10),
            description="`That` in LHS variable assignment"
        )
        self.expect_goto_definition_location(
            solc=solc,
            document_uri=FILE_URI,
            document_position=(15, 27),
            expected_uri=FILE_URI,
            expected_lineNo=4,
            expected_startEndColumns=(22, 26),
            description="`That` in expression"
        )

    def test_textDocument_didChange_empty_file(self, solc: JsonRpcProcess) -> None:
        """
        Starts with an empty file and changes it to look like
        the didOpen_with_import test case. Then we can use
        the same verification calls to ensure it worked as expected.
        """
        # This FILE_NAME must be alphabetically before lib.sol to not over-complify
        # the test logic in verify_didOpen_with_import_diagnostics.
        FILE_NAME = 'a_new_file'
        FILE_URI = self.get_test_file_uri(FILE_NAME)
        self.setup_lsp(solc)
        solc.send_message('textDocument/didOpen', {
            'textDocument': {
                'uri': FILE_URI,
                'languageId': 'Solidity',
                'version': 1,
                'text': ''
            }
        })
        reports = self.wait_for_diagnostics(solc, 1)
        self.expect_equal(len(reports), 1)
        report = reports[0]
        published_diagnostics = report['diagnostics']
        self.expect_equal(len(published_diagnostics), 2)
        self.expect_diagnostic(published_diagnostics[0], code=1878, lineNo=0, startEndColumns=(0, 0))
        self.expect_diagnostic(published_diagnostics[1], code=3420, lineNo=0, startEndColumns=(0, 0))
        solc.send_message('textDocument/didChange', {
            'textDocument': {
                'uri': self.get_test_file_uri('a_new_file')
            },
            'contentChanges': [
                {
                    'range': {
                        'start': { 'line': 0, 'character': 0 },
                        'end': { 'line': 0, 'character': 0 }
                    },
                    'text': self.get_test_file_contents('didOpen_with_import')
                }
            ]
        })
        published_diagnostics = self.wait_for_diagnostics(solc, 2)
        self.verify_didOpen_with_import_diagnostics(published_diagnostics, 'a_new_file')

    def test_textDocument_didChange_multi_line(self, solc: JsonRpcProcess) -> None:
        """
        Starts with an empty file and changes it to multiple times, changing
        content across lines.
        """
        self.setup_lsp(solc)
        FILE_NAME = 'didChange_template'
        FILE_URI = self.get_test_file_uri(FILE_NAME)
        solc.send_message('textDocument/didOpen', {
            'textDocument': {
                'uri': FILE_URI,
                'languageId': 'Solidity',
                'version': 1,
                'text': self.get_test_file_contents(FILE_NAME)
            }
        })
        published_diagnostics = self.wait_for_diagnostics(solc, 1)
        self.expect_equal(len(published_diagnostics), 1, "one publish diagnostics notification")
        self.expect_equal(len(published_diagnostics[0]['diagnostics']), 0, "no diagnostics")
        solc.send_message('textDocument/didChange', {
            'textDocument': { 'uri': FILE_URI },
            'contentChanges': [
                {
                    'range': {
                        'start': { 'line': 3, 'character': 3 },
                        'end': { 'line': 4, 'character': 1 }
                    },
                    'text': "tract D {\n\n  uint x\n = -1; \n "
                }
            ]
        })
        published_diagnostics = self.wait_for_diagnostics(solc, 1)
        self.expect_equal(len(published_diagnostics), 1, "one publish diagnostics notification")
        report2 = published_diagnostics[0]
        self.expect_equal(report2['uri'], FILE_URI, "Correct file URI")
        self.expect_equal(len(report2['diagnostics']), 1, "one diagnostic")
        self.expect_diagnostic(report2['diagnostics'][0], 7407, 6, (3, 5))

        # Now we are changing the part "x\n = -" of "uint x\n = -1;"
        solc.send_message('textDocument/didChange', {
            'textDocument': { 'uri': FILE_URI },
            'contentChanges': [
                {
                    'range': {
                        'start': { 'line': 5, 'character': 7 },
                        'end': { 'line': 6, 'character': 4 }
                    },
                    'text': "y\n = [\nuint(1),\n3,4]+"
                }
            ]
        })
        published_diagnostics = self.wait_for_diagnostics(solc, 1)
        self.expect_equal(len(published_diagnostics), 1, "one publish diagnostics notification")
        report3 = published_diagnostics[0]
        self.expect_equal(report3['uri'], FILE_URI, "Correct file URI")
        self.expect_equal(len(report3['diagnostics']), 2, "two diagnostics")
        diagnostic = report3['diagnostics'][0]
        self.expect_equal(diagnostic['code'], 2271, 'diagnostic: 2271')
        # check multi-line error code
        self.expect_equal(
            diagnostic['range'],
            {
                'end': {'character': 6, 'line': 8},
                'start': {'character': 3, 'line': 6}
            },
            "diagnostic: check range"
        )
        diagnostic = report3['diagnostics'][1]
        self.expect_equal(diagnostic['code'], 7407, 'diagnostic: 7407')
        # check multi-line error code
        self.expect_equal(
            diagnostic['range'],
            {
                'end': {'character': 6, 'line': 8},
                'start': {'character': 3, 'line': 6}
            },
            "diagnostic: check range"
        )

    # }}}
    # }}}

if __name__ == "__main__":
    suite = SolidityLSPTestSuite()
    exit_code = suite.main()
    sys.exit(exit_code)
