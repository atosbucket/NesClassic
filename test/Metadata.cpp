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
 * @date 2017
 * Metadata processing helpers.
 */

#include <string>
#include <iostream>
#include <libsolutil/Assertions.h>
#include <libsolutil/CommonData.h>
#include <test/Metadata.h>

using namespace std;

namespace solidity::test
{

bytes onlyMetadata(bytes const& _bytecode)
{
	size_t size = _bytecode.size();
	if (size < 5)
		return bytes{};
	size_t metadataSize = (static_cast<size_t>(_bytecode[size - 2]) << 8ul) + static_cast<size_t>(_bytecode[size - 1]);
	if (size < (metadataSize + 2))
		return bytes{};
	// Sanity check: assume the first byte is a fixed-size CBOR array with 1, 2 or 3 entries
	unsigned char firstByte = _bytecode[size - metadataSize - 2];
	if (firstByte != 0xa1 && firstByte != 0xa2 && firstByte != 0xa3)
		return bytes{};
	return bytes(_bytecode.end() - static_cast<ptrdiff_t>(metadataSize) - 2, _bytecode.end() - 2);
}

bytes bytecodeSansMetadata(bytes const& _bytecode)
{
	size_t metadataSize = onlyMetadata(_bytecode).size();
	if (metadataSize == 0)
		return bytes{};
	return bytes(_bytecode.begin(), _bytecode.end() - static_cast<ptrdiff_t>(metadataSize) - 2);
}

string bytecodeSansMetadata(string const& _bytecode)
{
	return util::toHex(bytecodeSansMetadata(fromHex(_bytecode, util::WhenError::Throw)));
}

DEV_SIMPLE_EXCEPTION(CBORException);

class TinyCBORParser
{
public:
	explicit TinyCBORParser(bytes const& _metadata): m_pos(0), m_metadata(_metadata)
	{
		assertThrow((m_pos + 1) < _metadata.size(), CBORException, "Input too short.");
	}
	unsigned mapItemCount()
	{
		assertThrow(nextType() == MajorType::Map, CBORException, "Fixed-length map expected.");
		return readLength();
	}
	string readKey()
	{
		return readString();
	}
	string readValue()
	{
		switch(nextType())
		{
			case MajorType::ByteString:
				return util::toHex(readBytes(readLength()));
			case MajorType::TextString:
				return readString();
			case MajorType::SimpleData:
			{
				unsigned value = nextImmediate();
				m_pos++;
				if (value == 20)
					return "false";
				else if (value == 21)
					return "true";
				else
					assertThrow(false, CBORException, "Unsupported simple value (not a boolean).");
			}
			default:
				assertThrow(false, CBORException, "Unsupported value type.");
		}
	}
private:
	enum class MajorType
	{
		ByteString,
		TextString,
		Map,
		SimpleData
	};
	MajorType nextType() const
	{
		unsigned value = (m_metadata.at(m_pos) >> 5) & 0x7;
		switch (value)
		{
			case 2: return MajorType::ByteString;
			case 3: return MajorType::TextString;
			case 5: return MajorType::Map;
			case 7: return MajorType::SimpleData;
			default: assertThrow(false, CBORException, "Unsupported major type.");
		}
	}
	unsigned nextImmediate() const { return m_metadata.at(m_pos) & 0x1f; }
	unsigned readLength()
	{
		unsigned length = m_metadata.at(m_pos++) & 0x1f;
		if (length < 24)
			return length;
		if (length == 24)
			return m_metadata.at(m_pos++);
		// Unsupported length kind. (Only by this parser.)
		assertThrow(false, CBORException, string("Unsupported length ") + to_string(length));
	}
	bytes readBytes(unsigned length)
	{
		bytes ret{m_metadata.begin() + m_pos, m_metadata.begin() + m_pos + length};
		m_pos += length;
		return ret;
	}
	string readString()
	{
		// Expect a text string.
		assertThrow(nextType() == MajorType::TextString, CBORException, "String expected.");
		bytes tmp{readBytes(readLength())};
		return string{tmp.begin(), tmp.end()};
	}
	unsigned m_pos;
	bytes const& m_metadata;
};

std::optional<map<string, string>> parseCBORMetadata(bytes const& _metadata)
{
	try
	{
		TinyCBORParser parser(_metadata);
		map<string, string> ret;
		unsigned count = parser.mapItemCount();
		for (unsigned i = 0; i < count; i++)
		{
			string key = parser.readKey();
			string value = parser.readValue();
			ret[move(key)] = move(value);
		}
		return ret;
	}
	catch (CBORException const&)
	{
		return {};
	}
}

bool isValidMetadata(string const& _serialisedMetadata)
{
	Json::Value metadata;
	if (!util::jsonParseStrict(_serialisedMetadata, metadata))
		return false;

	return isValidMetadata(metadata);
}

bool isValidMetadata(Json::Value const& _metadata)
{
	if (
		!_metadata.isObject() ||
		!_metadata.isMember("version") ||
		!_metadata.isMember("language") ||
		!_metadata.isMember("compiler") ||
		!_metadata.isMember("settings") ||
		!_metadata.isMember("sources") ||
		!_metadata.isMember("output") ||
		!_metadata["settings"].isMember("evmVersion") ||
		!_metadata["settings"].isMember("metadata") ||
		!_metadata["settings"]["metadata"].isMember("bytecodeHash")
	)
		return false;

	if (!_metadata["version"].isNumeric() || _metadata["version"] != 1)
		return false;

	if (!_metadata["language"].isString() || _metadata["language"].asString() != "Solidity")
		return false;

	/// @TODO add more strict checks

	return true;
}

} // end namespaces
