#!/usr/bin/env bash

# ------------------------------------------------------------------------------
# This file is part of solidity.
#
# solidity is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# solidity is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with solidity.  If not, see <http://www.gnu.org/licenses/>
#
# (c) 2019 solidity contributors.
#------------------------------------------------------------------------------

set -e

source scripts/common.sh
source test/externalTests/common.sh

REPO_ROOT=$(realpath "$(dirname "$0")/../..")

verify_input "$@"
BINARY_TYPE="$1"
BINARY_PATH="$2"
SELECTED_PRESETS="$3"

function compile_fn { npm run compile; }
function test_fn { npm test; }

function zeppelin_test
{
    local repo="https://github.com/OpenZeppelin/openzeppelin-contracts.git"
    local ref_type=branch
    local ref="master"
    local config_file="hardhat.config.js"

    local compile_only_presets=()
    local settings_presets=(
        "${compile_only_presets[@]}"
        #ir-no-optimize           # Compilation fails with "YulException: Variable var_account_852 is 4 slot(s) too deep inside the stack."
        #ir-optimize-evm-only     # Compilation fails with "YulException: Variable var_account_852 is 4 slot(s) too deep inside the stack."
        ir-optimize-evm+yul
        legacy-no-optimize
        legacy-optimize-evm-only
        legacy-optimize-evm+yul
    )

    [[ $SELECTED_PRESETS != "" ]] || SELECTED_PRESETS=$(circleci_select_steps_multiarg "${settings_presets[@]}")
    print_presets_or_exit "$SELECTED_PRESETS"

    setup_solc "$DIR" "$BINARY_TYPE" "$BINARY_PATH"
    download_project "$repo" "$ref_type" "$ref" "$DIR"

    # Disable tests that won't pass on the ir presets due to Hardhat heuristics. Note that this also disables
    # them for other presets but that's fine - we want same code run for benchmarks to be comparable.
    # TODO: Remove this when Hardhat adjusts heuristics for IR (https://github.com/nomiclabs/hardhat/issues/2115).
    pushd test/utils/
    sed -i "s|it(\('reverts \)|it.skip(\1|g" math/SafeMath.test.js
    sed -i "s|it(\('reverts \)|it.skip(\1|g" math/SignedSafeMath.test.js
    sed -i "s|it(\('reverts \)|it.skip(\1|g" structs/EnumerableSet.behavior.js
    popd

    # In some cases Hardhat does not detect revert reasons properly via IR.
    # TODO: Remove this when https://github.com/NomicFoundation/hardhat/issues/2453 gets fixed.
    sed -i "s|it(\('reverts if the current value is 0'\)|it.skip(\1|g" test/utils/Counters.test.js
    # TODO: Remove this when https://github.com/NomicFoundation/hardhat/issues/2115 gets fixed.
    sed -i "s|describe\(('Polygon-Child'\)|describe.skip\1|g" test/crosschain/CrossChainEnabled.test.js

    neutralize_package_json_hooks
    force_hardhat_compiler_binary "$config_file" "$BINARY_TYPE" "$BINARY_PATH"
    force_hardhat_compiler_settings "$config_file" "$(first_word "$SELECTED_PRESETS")"
    npm install

    replace_version_pragmas

    for preset in $SELECTED_PRESETS; do
        hardhat_run_test "$config_file" "$preset" "${compile_only_presets[*]}" compile_fn test_fn
        store_benchmark_report hardhat zeppelin "$repo" "$preset"
    done
}

external_test Zeppelin zeppelin_test
