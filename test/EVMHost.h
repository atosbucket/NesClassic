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
/**
 * EVM execution host, i.e. component that implements a simulated Ethereum blockchain
 * for testing purposes.
 */

#pragma once

#include <test/evmc/evmc.hpp>
#include <test/evmc/evmc.h>

#include <liblangutil/EVMVersion.h>

#include <libdevcore/FixedHash.h>

namespace dev
{
namespace test
{
using Address = h160;

class EVMHost: public evmc::Host
{
public:
	/// Tries to dynamically load libevmone. @returns nullptr on failure.
	/// The path has to be provided for the first successful run and will be ignored
	/// afterwards.
	static evmc::VM& getVM(std::string const& _path = {});

	explicit EVMHost(langutil::EVMVersion _evmVersion, evmc::VM& _vm = getVM());

	struct Account
	{
		evmc::uint256be balance = {};
		size_t nonce = 0;
		bytes code;
		evmc::bytes32 codeHash = {};
		std::map<evmc::bytes32, evmc::bytes32> storage;
	};

	struct LogEntry
	{
		Address address;
		std::vector<h256> topics;
		bytes data;
	};

	struct State
	{
		size_t blockNumber;
		uint64_t timestamp;
		std::map<evmc::address, Account> accounts;
		std::vector<LogEntry> logs;
	};

	Account const* account(evmc::address const& _address) const
	{
		auto it = m_state.accounts.find(_address);
		return it == m_state.accounts.end() ? nullptr : &it->second;
	}

	Account* account(evmc::address const& _address)
	{
		auto it = m_state.accounts.find(_address);
		return it == m_state.accounts.end() ? nullptr : &it->second;
	}

	void reset() { m_state = State{}; m_currentAddress = {}; }
	void newBlock()
	{
		m_state.blockNumber++;
		m_state.timestamp += 15;
		m_state.logs.clear();
	}

	bool account_exists(evmc::address const& _addr) const noexcept final
	{
		return account(_addr) != nullptr;
	}

	evmc::bytes32 get_storage(evmc::address const& _addr, evmc::bytes32 const& _key) const noexcept final
	{
		if (auto* acc = account(_addr))
		{
			auto it = acc->storage.find(_key);
			if (it != acc->storage.end())
				return it->second;
		}
		return {};
	}

	evmc_storage_status set_storage(
		evmc::address const& _addr,
		evmc::bytes32 const& _key,
		evmc::bytes32 const& _value
	) noexcept;

	evmc::uint256be get_balance(evmc::address const& _addr) const noexcept final
	{
		if (Account const* acc = account(_addr))
			return acc->balance;
		return {};
	}

	size_t get_code_size(evmc::address const& _addr) const noexcept final
	{
		if (Account const* acc = account(_addr))
			return acc->code.size();
		return 0;
	}

	evmc::bytes32 get_code_hash(evmc::address const& _addr) const noexcept final
	{
		if (Account const* acc = account(_addr))
			return acc->codeHash;
		return {};
	}

	size_t copy_code(
		evmc::address const& _addr,
		size_t _codeOffset,
		uint8_t* _bufferData,
		size_t _bufferSize
	) const noexcept final
	{
		size_t i = 0;
		if (Account const* acc = account(_addr))
			for (; i < _bufferSize && _codeOffset + i < acc->code.size(); i++)
				_bufferData[i] = acc->code[_codeOffset + i];
		return i;
	}

	void selfdestruct(evmc::address const& _addr, evmc::address const& _beneficiary) noexcept;

	evmc::result call(evmc_message const& _message) noexcept;

	evmc_tx_context get_tx_context() const noexcept;

	evmc::bytes32 get_block_hash(int64_t number) const noexcept;

	void emit_log(
		evmc::address const& _addr,
		uint8_t const* _data,
		size_t _dataSize,
		evmc::bytes32 const _topics[],
		size_t _topicsCount
	) noexcept;

	static Address convertFromEVMC(evmc::address const& _addr);
	static evmc::address convertToEVMC(Address const& _addr);
	static h256 convertFromEVMC(evmc::bytes32 const& _data);
	static evmc::bytes32 convertToEVMC(h256 const& _data);


	State m_state;
	evmc::address m_currentAddress = {};
	evmc::address m_coinbase = convertToEVMC(Address("0x7878787878787878787878787878787878787878"));

private:
	static evmc::result precompileECRecover(evmc_message const& _message) noexcept;
	static evmc::result precompileSha256(evmc_message const& _message) noexcept;
	static evmc::result precompileRipeMD160(evmc_message const& _message) noexcept;
	static evmc::result precompileIdentity(evmc_message const& _message) noexcept;
	static evmc::result precompileModExp(evmc_message const& _message) noexcept;
	static evmc::result precompileALTBN128G1Add(evmc_message const& _message) noexcept;
	static evmc::result precompileALTBN128G1Mul(evmc_message const& _message) noexcept;
	static evmc::result precompileALTBN128PairingProduct(evmc_message const& _message) noexcept;
	static evmc::result precompileGeneric(evmc_message const& _message, std::map<bytes, bytes> const& _inOut) noexcept;
	/// @returns a result object with no gas usage and result data taken from @a _data.
	/// @note The return value is only valid as long as @a _data is alive!
	static evmc::result resultWithGas(evmc_message const& _message, bytes const& _data) noexcept;

	evmc::VM& m_vm;
	// EVM version requested by the testing tool
	langutil::EVMVersion m_evmVersion;
	// EVM version requested from EVMC (matches the above)
	evmc_revision m_evmRevision;
};


}
}
