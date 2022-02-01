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
# (c) 2022 solidity contributors.
#------------------------------------------------------------------------------

set -e

source scripts/common.sh
source test/externalTests/common.sh

verify_input "$@"
BINARY_TYPE="$1"
BINARY_PATH="$2"
SELECTED_PRESETS="$3"

function compile_fn { npm run build; }
function test_fn { npm run test; }

function elementfi_test
{
    local repo="https://github.com/element-fi/elf-contracts"
    local ref_type=branch
    local ref=main
    local config_file="hardhat.config.ts"
    local config_var=config

    local compile_only_presets=(
        ir-optimize-evm+yul       # Compiles but tests fail. See https://github.com/nomiclabs/hardhat/issues/2115
    )
    local settings_presets=(
        "${compile_only_presets[@]}"
        #ir-no-optimize           # Compilation fails with "YulException: Variable var_amount_9311 is 10 slot(s) too deep inside the stack."
        #ir-optimize-evm-only     # Compilation fails with "YulException: Variable var_amount_9311 is 10 slot(s) too deep inside the stack."
        legacy-no-optimize
        legacy-optimize-evm-only
        legacy-optimize-evm+yul
    )

    [[ $SELECTED_PRESETS != "" ]] || SELECTED_PRESETS=$(circleci_select_steps_multiarg "${settings_presets[@]}")
    print_presets_or_exit "$SELECTED_PRESETS"

    setup_solc "$DIR" "$BINARY_TYPE" "$BINARY_PATH"
    download_project "$repo" "$ref_type" "$ref" "$DIR"

    chmod +x scripts/load-balancer-contracts.sh
    scripts/load-balancer-contracts.sh

    # Balancer contracts require 0.7.x. Patch them for 0.8.x.
    pushd contracts/balancer-core-v2
    sed -i 's|uint256(address(this))|uint256(uint160(address(this)))|g' test/MockPoolFactory.sol
    sed -i 's|uint256(address(this))|uint256(uint160(address(this)))|g' vault/ProtocolFeesCollector.sol
    sed -i 's|uint256(address(this))|uint256(uint160(address(this)))|g' vault/VaultAuthorization.sol
    sed -i 's|msg\.sender\.sendValue(excess)|payable(msg.sender).sendValue(excess)|g' vault/AssetTransfersHandler.sol
    sed -i 's|msg\.sender\.transfer(wad)|payable(msg.sender).transfer(wad)|g' test/WETH.sol
    sed -i 's|int256(-amountsOut\[i\])|-int256(amountsOut[i])|g' test/MockVault.sol
    sed -i 's|int256(-amount)|-int256(amount)|g' vault/AssetManagers.sol
    sed -i 's|uint256(-1)|type(uint256).max|g' pools/BalancerPoolToken.sol
    sed -i 's|uint256(-1)|type(uint256).max|g' test/WETH.sol
    sed -i 's|IERC20(0)|IERC20(address(0))|g' pools/BasePool.sol
    sed -i 's|IERC20(0)|IERC20(address(0))|g' vault/balances/TwoTokenPoolsBalance.sol
    sed -i 's|IERC20(0)|IERC20(address(0))|g' vault/FlashLoans.sol
    sed -i 's|IERC20(0)|IERC20(address(0))|g' vault/PoolTokens.sol
    sed -i 's|uint256(msg\.sender)|uint256(uint160(msg.sender))|g' pools/BasePool.sol
    sed -i 's|uint256(msg\.sender)|uint256(uint160(msg.sender))|g' pools/weighted/WeightedPool2Tokens.sol
    sed -i 's|address(uint256(_data))|address(uint160(uint256(_data)))|g' lib/openzeppelin/Create2.sol
    sed -i 's|address(uint256(poolId) >> (12 \* 8))|address(uint160(uint256(poolId) >> (12 * 8)))|g' vault/PoolRegistry.sol
    sed -i 's|bytes32(uint256(pool))|bytes32(uint256(uint160(pool)))|g' vault/PoolRegistry.sol
    popd

    # Several tests fail unless we use the exact versions hard-coded in package-lock.json
    #neutralize_package_lock

    neutralize_package_json_hooks
    force_hardhat_compiler_binary "$config_file" "$BINARY_TYPE" "$BINARY_PATH"
    force_hardhat_compiler_settings "$config_file" "$(first_word "$SELECTED_PRESETS")" "$config_var"
    force_hardhat_unlimited_contract_size "$config_file" "$config_var"
    npm install

    replace_version_pragmas

    for preset in $SELECTED_PRESETS; do
        hardhat_run_test "$config_file" "$preset" "${compile_only_presets[*]}" compile_fn test_fn "$config_var"
    done
}

external_test ElementFi elementfi_test
