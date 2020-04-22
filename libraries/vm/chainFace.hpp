#pragma once

#include "Instruction.hpp"
#include "evm/evm.hpp"
#include "common/vector_ref.hpp"
#include <graphene/chain/protocol/types.hpp>
#include <graphene/chain/protocol/block.hpp>


namespace Yoyow
{
namespace vm
{
using namespace graphene::chain;
using namespace dev;

class chainVMFace;
class chainVM;
using OnOpFunc = std::function<void(uint64_t /*steps*/, uint64_t /* PC */, Instruction /*instr*/,
                                    fc::bigint /*newMemSize*/, fc::bigint /*gasCost*/, fc::bigint /*gas*/, chainVM const*, chainVMFace const*)>;

struct CallParameters
{
    CallParameters() = default;
    CallParameters(Address _senderAddress, Address _codeAddress, Address _receiveAddress,
        u256 _valueTransfer, u256 _apparentValue, u256 _gas, bytes _data,
        OnOpFunc _onOpFunc)
      : senderAddress(_senderAddress),
        codeAddress(_codeAddress),
        receiveAddress(_receiveAddress),
        valueTransfer(_valueTransfer),
        apparentValue(_apparentValue),
        gas(_gas),
        data(_data),
        onOp(_onOpFunc)
    {}
    Address senderAddress;
    Address codeAddress;
    Address receiveAddress;
    u256 valueTransfer;
    u256 apparentValue;
    u256 gas;
    bytes data;
    bool staticCall = false;
    OnOpFunc onOp;
};
struct VMnvInfo
{
public:
    VMnvInfo(block_header const& _current, u256 const& _gasUsed)
      : _block_header(_current), _gasUsed(_gasUsed)
    {}
    block_header              _block_header;
    u256                      _gasUsed;
};

/// Represents a call result.
///
/// @todo: Replace with evmc_result in future.
struct CallResult
{
    evmc_status_code status;
    owning_bytes_ref output;

    CallResult(evmc_status_code status, owning_bytes_ref&& output)
      : status{status}, output{std::move(output)}
    {}
};

/// Represents a CREATE result.
///
/// @todo: Replace with evmc_result in future.
struct CreateResult
{
    evmc_status_code status;
    owning_bytes_ref output;
    Address address;

    CreateResult(evmc_status_code status, owning_bytes_ref&& output, Address const& address)
      : status{status}, output{std::move(output)}, address{address}
    {}
};

/**
 * @brief Interface and null implementation of the class for specifying VM externalities.
 */

class chainVMFace
{
public:
    /// Full constructor.
    chainVMFace(VMnvInfo const& _envInfo, Address _myAddress, Address _caller, Address _origin,
        u256 _value, u256 _gasPrice, bytes _data, bytes _code, fc::sha256 const& _codeHash,
        unsigned _depth, bool _isCreate, bool _staticCall);

    chainVMFace(chainVMFace const&) = delete;
    chainVMFace& operator=(chainVMFace const&) = delete;

    virtual ~chainVMFace() = default;

    /// Read storage location.
    virtual u256 store(u256) { return 0; }

    /// Write a value in storage.
    virtual void setStore(u256, u256) {}

    /// Read original storage value (before modifications in the current transaction).
    virtual u256 originalStorageValue(u256 const&) { return 0; }

    /// Read address's balance.
    virtual u256 balance(Address) { return 0; }

    /// Read address's code.
    virtual bytes const& codeAt(Address) { return bytes(); }

    /// @returns the size of the code in bytes at the given address.
    virtual size_t codeSizeAt(Address) { return 0; }

    /// @returns the hash of the code at the given address.
    virtual h256 codeHashAt(Address) { return h256{}; }

    /// Does the account exist?
    virtual bool exists(Address) { return false; }

    /// Suicide the associated contract and give proceeds to the given address.
    virtual void suicide(Address) { /*sub.suicides.insert(myAddress);*/ }

    /// Create a new (contract) account.
    virtual CreateResult create(u256, u256&, bytes, Instruction, u256, OnOpFunc const&) = 0;

    /// Make a new message call.
    virtual CallResult call(CallParameters&) = 0;

    /// Revert any changes made (by any of the other calls).
    virtual void log(h256s&& _topics, bytes _data)
    {
        //sub.logs.push_back(LogEntry(myAddress, std::move(_topics), _data.toBytes()));
    }

    /// Hash of a block if within the last 256 blocks, or h256() otherwise.
    virtual h256 blockHash(u256 _number) = 0;

    /// Get the execution environment information.
    VMnvInfo const& envInfo() const { return m_envInfo; }

    /// Return the VM gas-price schedule for this execution context.
    //virtual VMSchedule const& evmSchedule() const { return DefaultSchedule; }

private:
    VMnvInfo const& m_envInfo;

public:

    Address    myAddress;  ///< Address associated with executing code (a contract, or contract-to-be).
    Address    caller;     ///< Address which sent the message (either equal to origin or a contract).
    Address    origin;     ///< Original transactor.
    u256       value;         ///< Value (in Wei) that was passed to this address.
    u256       gasPrice;      ///< Price of gas (that we already paid).
    bytes      data;       ///< Current input data.
    bytes      code;               ///< Current code that is executing.
    fc::sha256 codeHash;            ///< SHA3 hash of the executing code
    u256       salt;                ///< Values used in new address construction by CREATE2
    //SubState   sub;             ///< Sub-band VM state (suicides, refund counter, logs).
    unsigned   depth = 0;       ///< Depth of the present call.
    bool       isCreate = false;    ///< Is this a CREATE call?
    bool       staticCall = false;  ///< Throw on state changing.
};

class chainVM :public chainVMFace{
public:
   
};
}
}

