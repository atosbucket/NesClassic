#!/usr/bin/env bash

#------------------------------------------------------------------------------
# Bash script for building Solidity for emscripten.
#
# The documentation for solidity is hosted at:
#
#     https://docs.soliditylang.org
#
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
# (c) 2016 solidity contributors.
#------------------------------------------------------------------------------

set -e

if test -z "$1"; then
    BUILD_DIR="emscripten_build"
else
    BUILD_DIR="$1"
fi

# solbuildpackpusher/solidity-buildpack-deps:emscripten-6
docker run -v "$(pwd):/root/project" -w /root/project \
    solbuildpackpusher/solidity-buildpack-deps@sha256:092da5817bc032c91a806b4f73db2a1a31e5cc4c066d94d43eedd9f365df7154 \
    ./scripts/ci/build_emscripten.sh "$BUILD_DIR"
