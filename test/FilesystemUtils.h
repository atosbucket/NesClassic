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
 * Helpers for common filesystem operations used in multiple tests.
 */

#pragma once

#include <boost/filesystem.hpp>

#include <string>

namespace solidity::test
{

/// Creates a file with the exact content specified in the second argument.
/// Throws an exception if the file already exists or if the parent directory of the file does not.
void createFileWithContent(boost::filesystem::path const& _path, std::string const& content);

/// Creates a symlink between two paths.
/// The target does not have to exist.
/// @returns true if the symlink has been successfully created, false if the filesystem does not
/// support symlinks.
/// Throws an exception of the operation fails for a different reason.
bool createSymlinkIfSupportedByFilesystem(
	boost::filesystem::path const& _targetPath,
	boost::filesystem::path const& _linkName
);

}
