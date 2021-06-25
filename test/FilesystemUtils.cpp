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

#include <test/FilesystemUtils.h>

#include <test/libsolidity/util/SoltestErrors.h>

using namespace std;
using namespace solidity;
using namespace solidity::test;

void solidity::test::createFileWithContent(boost::filesystem::path const& _path, string const& content)
{
	if (boost::filesystem::is_regular_file(_path))
		BOOST_THROW_EXCEPTION(runtime_error("File already exists: \"" + _path.string() + "\".")); \

	// Use binary mode to avoid line ending conversion on Windows.
	ofstream newFile(_path.string(), std::ofstream::binary);
	if (newFile.fail() || !boost::filesystem::is_regular_file(_path))
		BOOST_THROW_EXCEPTION(runtime_error("Failed to create a file: \"" + _path.string() + "\".")); \

	newFile << content;
}

bool solidity::test::createSymlinkIfSupportedByFilesystem(
	boost::filesystem::path const& _targetPath,
	boost::filesystem::path const& _linkName
)
{
	boost::system::error_code symlinkCreationError;
	boost::filesystem::create_symlink(_targetPath, _linkName, symlinkCreationError);

	if (!symlinkCreationError)
		return true;
	else if (
		symlinkCreationError == boost::system::errc::not_supported ||
		symlinkCreationError == boost::system::errc::operation_not_supported
	)
		return false;
	else
		BOOST_THROW_EXCEPTION(runtime_error(
			"Failed to create a symbolic link: \"" + _linkName.string() + "\""
			" -> " + _targetPath.string() + "\"."
		));
}
