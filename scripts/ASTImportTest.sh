#!/usr/bin/env bash

set -e

# Bash script to test the ast-import option of the compiler by
# first exporting a .sol file to JSON, then loading it into the compiler
# and exporting it again. The second JSON should be identical to the first
READLINK=readlink
if [[ "$OSTYPE" == "darwin"* ]]; then
    READLINK=greadlink
fi
REPO_ROOT=$(${READLINK} -f "$(dirname "$0")"/..)
SOLIDITY_BUILD_DIR=${SOLIDITY_BUILD_DIR:-${REPO_ROOT}/build}
SOLC=${SOLIDITY_BUILD_DIR}/solc/solc
SPLITSOURCES=${REPO_ROOT}/scripts/splitSources.py

SYNTAXTESTS_DIR="${REPO_ROOT}/test/libsolidity/syntaxTests"
ASTJSONTESTS_DIR="${REPO_ROOT}/test/libsolidity/ASTJSON"
NSOURCES="$(find "$SYNTAXTESTS_DIR" -type f | wc -l)"

# DEV_DIR="${REPO_ROOT}/../tmp/contracts/"
# NSOURCES="$(find $DEV_DIR -type f | wc -l)" #TODO use find command

FAILED=0
UNCOMPILABLE=0
TESTED=0

if [[ "$(find . -maxdepth 0 -type d -empty)" == "" ]]; then
    echo "Test directory not empty. Skipping!"
    exit 1
fi

# function tests whether exporting and importing again leaves the JSON ast unchanged
# Results are recorded by adding to FAILED or UNCOMPILABLE.
# Also, in case of a mismatch a diff and the respective ASTs are printed
# Expected parameters:
# $1 name of the file to be exported and imported
# $2 any files needed to do so that might be in parent directories
function testImportExportEquivalence {
    local nth_input_file="$1"
    IFS=" " read -r -a all_input_files <<< "$2"

    if $SOLC "$nth_input_file" "${all_input_files[@]}" > /dev/null 2>&1
    then
        ! [[ -e stderr.txt ]] || { echo "stderr.txt already exists. Refusing to overwrite."; exit 1; }

        # save exported json as expected result (silently)
        $SOLC --combined-json ast --pretty-json "$nth_input_file" "${all_input_files[@]}" > expected.json 2> /dev/null
        # import it, and export it again as obtained result (silently)
        if ! $SOLC --import-ast --combined-json ast --pretty-json expected.json > obtained.json 2> stderr.txt
        then
            # For investigating, use exit 1 here so the script stops at the
            # first failing test
            # exit 1
            FAILED=$((FAILED + 1))
            echo -e "ERROR: AST reimport failed for input file $nth_input_file"
            echo
            echo "Compiler stderr:"
            cat ./stderr.txt
            echo
            echo "Compiler stdout:"
            cat ./obtained.json
            return 1
        fi
        DIFF="$(diff expected.json obtained.json)"
        if [ "$DIFF" != "" ]
        then
            if [ "$DIFFVIEW" == "" ]
            then
                echo -e "ERROR: JSONS differ for $1: \n $DIFF \n"
                echo "Expected:"
                cat ./expected.json
                echo "Obtained:"
                cat ./obtained.json
            else
                # Use user supplied diff view binary
                $DIFFVIEW expected.json obtained.json
            fi
            FAILED=$((FAILED + 1))
            return 2
        fi
        TESTED=$((TESTED + 1))
        rm expected.json obtained.json
        rm -f stderr.txt
    else
        # echo "contract $solfile could not be compiled "
        UNCOMPILABLE=$((UNCOMPILABLE + 1))
    fi
    # return 0
}
echo "Looking at $NSOURCES .sol files..."

WORKINGDIR=$PWD

# for solfile in $(find $DEV_DIR -name *.sol)
# boost_filesystem_bug specifically tests a local fix for a boost::filesystem
# bug. Since the test involves a malformed path, there is no point in running
# AST tests on it. See https://github.com/boostorg/filesystem/issues/176
# shellcheck disable=SC2044
for solfile in $(find "$SYNTAXTESTS_DIR" "$ASTJSONTESTS_DIR" -name "*.sol" -and -not -name "boost_filesystem_bug.sol")
do
    echo -n "."
    # create a temporary sub-directory
    FILETMP=$(mktemp -d)
    cd "$FILETMP"

    set +e
    OUTPUT=$("$SPLITSOURCES" "$solfile")
    SPLITSOURCES_RC=$?
    set -e
    if [ ${SPLITSOURCES_RC} == 0 ]
    then
        # echo $OUTPUT
        NSOURCES=$((NSOURCES - 1))
        for i in $OUTPUT;
        do
            testImportExportEquivalence "$i" "$OUTPUT"
            NSOURCES=$((NSOURCES + 1))
        done
    elif [ ${SPLITSOURCES_RC} == 1 ]
    then
        testImportExportEquivalence "$solfile"
    elif [ ${SPLITSOURCES_RC} == 2 ]
    then
        # The script will exit with return code 2, if an UnicodeDecodeError occurred.
        # This is the case if e.g. some tests are using invalid utf-8 sequences. We will ignore
        # these errors, but print the actual output of the script.
        echo -e "\n${OUTPUT}\n"
        testImportExportEquivalence "$solfile"
    else
        # All other return codes will be treated as critical errors. The script will exit.
        echo -e "\nGot unexpected return code ${SPLITSOURCES_RC} from ${SPLITSOURCES}. Aborting."
        echo -e "\n${OUTPUT}\n"

        cd "$WORKINGDIR"
        # Delete temporary files
        rm -rf "$FILETMP"

        exit 1
    fi

    cd "$WORKINGDIR"
    # Delete temporary files
    rm -rf "$FILETMP"
done

echo ""

if [ "$FAILED" = 0 ]
then
    echo "SUCCESS: $TESTED syntaxTests passed, $FAILED failed, $UNCOMPILABLE could not be compiled ($NSOURCES sources total)."
else
    echo "FAILURE: Out of $NSOURCES sources, $FAILED failed, ($UNCOMPILABLE could not be compiled)."
    exit 1
fi
