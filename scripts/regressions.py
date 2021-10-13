#!/usr/bin/env python3

from argparse import ArgumentParser
import sys
import os
import subprocess
import re
import glob
import threading
import time

DESCRIPTION = """Regressor is a tool to run regression tests in a CI env."""

class PrintDotsThread:
    """Prints a dot every "interval" (default is 300) seconds"""

    def __init__(self, interval=300):
        self.interval = interval

        thread = threading.Thread(target=self.run, args=())
        thread.daemon = True
        thread.start()

    def run(self):
        """ Runs until the main Python thread exits. """
        ## Print a newline at the very beginning.
        print("")
        while True:
            # Print dot
            print(".")
            time.sleep(self.interval)

class regressor:
    _re_sanitizer_log = re.compile(r"""ERROR: (libFuzzer|UndefinedBehaviorSanitizer)""")

    def __init__(self, description, args):
        self._description = description
        self._args = self.parseCmdLine(description, args)
        self._repo_root = os.path.dirname(sys.path[0])
        self._fuzzer_path = os.path.join(self._repo_root,
                                         "build/test/tools/ossfuzz")
        self._logpath = os.path.join(self._repo_root, "test_results")

    def parseCmdLine(self, description, args):
        argParser = ArgumentParser(description)
        argParser.add_argument('-o', '--out-dir', required=True, type=str,
                               help="""Directory where test results will be written""")
        return argParser.parse_args(args)

    @staticmethod
    def run_cmd(command, logfile=None, env=None):
        """
        Args:
            command (str): command to run
            logfile (str): log file name
            env (dict): dictionary holding key-value pairs for bash environment
                    variables

        Returns:
            int: The exit status of the command. Exit status codes are:
                0       -> Success
                1-255   -> Failure
        """
        if not logfile:
            logfile = os.devnull

        if not env:
            env = os.environ.copy()

        with open(logfile, 'w', encoding='utf8') as logfh:
            with subprocess.Popen(command, shell=True, executable='/bin/bash',
                                    env=env, stdout=logfh,
                                    stderr=subprocess.STDOUT) as proc:
                ret = proc.wait()
                logfh.close()
                return ret

    def process_log(self, logfile):
        """
        Args:
            logfile (str): log file name

        Returns:
            bool: Test status.
                True       -> Success
                False      -> Failure
        """

        ## Log may contain non ASCII characters, so we simply stringify them
        ## since they don't matter for regular expression matching
        with open(logfile, 'rb', encoding=None) as f:
            rawtext = str(f.read())
        return not re.search(self._re_sanitizer_log, rawtext)

    def run(self):
        """
        Returns:
            bool: Test status.
                True       -> All tests succeeded
                False      -> At least one test failed
        """

        testStatus = []
        for fuzzer in glob.iglob(f"{self._fuzzer_path}/*_ossfuzz"):
            basename = os.path.basename(fuzzer)
            logfile = os.path.join(self._logpath, f"{basename}.log")
            corpus_dir = f"/tmp/solidity-fuzzing-corpus/{basename}_seed_corpus"
            cmd = f"find {corpus_dir} -type f | xargs -n1 sh -c '{fuzzer} $0 || exit 255'"
            self.run_cmd(cmd, logfile=logfile)
            ret = self.process_log(logfile)
            if not ret:
                print(
                    f"\t[-] libFuzzer reported failure for {basename}. "
                    "Failure logged to test_results")
                testStatus.append(False)
            else:
                print(f"\t[+] {basename} passed regression tests.")
                testStatus.append(True)
        return all(testStatus)


if __name__ == '__main__':
    dotprinter = PrintDotsThread()
    tool = regressor(DESCRIPTION, sys.argv[1:])
    sys.exit(not tool.run())
