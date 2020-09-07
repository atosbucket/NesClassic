.. index:: ! functions

.. _functions:

*********
Functions
*********

Functions can be defined inside and outside of contracts.

Functions outside of a contract, also called "free functions", always have implicit ``internal``
:ref:`visibility<visibility-and-getters>`. Their code is included in all contracts
that call them, similar to internal library functions.

::

    // SPDX-License-Identifier: GPL-3.0
    pragma solidity >0.7.0 <0.8.0;

    function sum(uint[] memory _arr) pure returns (uint s) {
        for (uint i = 0; i < _arr.length; i++)
            s += _arr[i];
    }

    contract ArrayExample {
        bool found;
        function f(uint[] memory _arr) public {
            // This calls the free function internally.
            // The compiler will add its code to the contract.
            uint s = sum(_arr);
            require(s >= 10);
            found = true;
        }
    }


.. _function-parameters-return-variables:

Function Parameters and Return Variables
========================================

Functions take typed parameters as input and may, unlike in many other
languages, also return an arbitrary number of values as output.

Function Parameters
-------------------

Function parameters are declared the same way as variables, and the name of
unused parameters can be omitted.

For example, if you want your contract to accept one kind of external call
with two integers, you would use something like the following::

    // SPDX-License-Identifier: GPL-3.0
    pragma solidity >=0.4.16 <0.8.0;

    contract Simple {
        uint sum;
        function taker(uint _a, uint _b) public {
            sum = _a + _b;
        }
    }

Function parameters can be used as any other local variable and they can also be assigned to.

.. note::

  An :ref:`external function<external-function-calls>` cannot accept a
  multi-dimensional array as an input
  parameter. This functionality is possible if you enable the new
  ``ABIEncoderV2`` feature by adding ``pragma experimental ABIEncoderV2;`` to your source file.

  An :ref:`internal function<external-function-calls>` can accept a
  multi-dimensional array without enabling the feature.

.. index:: return array, return string, array, string, array of strings, dynamic array, variably sized array, return struct, struct

Return Variables
----------------

Function return variables are declared with the same syntax after the
``returns`` keyword.

For example, suppose you want to return two results: the sum and the product of
two integers passed as function parameters, then you use something like::

    // SPDX-License-Identifier: GPL-3.0
    pragma solidity >=0.4.16 <0.8.0;

    contract Simple {
        function arithmetic(uint _a, uint _b)
            public
            pure
            returns (uint o_sum, uint o_product)
        {
            o_sum = _a + _b;
            o_product = _a * _b;
        }
    }

The names of return variables can be omitted.
Return variables can be used as any other local variable and they
are initialized with their :ref:`default value <default-value>` and have that
value until they are (re-)assigned.

You can either explicitly assign to return variables and
then leave the function as above,
or you can provide return values
(either a single or :ref:`multiple ones<multi-return>`) directly with the ``return``
statement::

    // SPDX-License-Identifier: GPL-3.0
    pragma solidity >=0.4.16 <0.8.0;

    contract Simple {
        function arithmetic(uint _a, uint _b)
            public
            pure
            returns (uint o_sum, uint o_product)
        {
            return (_a + _b, _a * _b);
        }
    }

If you use an early ``return`` to leave a function that has return variables,
you must provide return values together with the return statement.

.. note::
    You cannot return some types from non-internal functions, notably
    multi-dimensional dynamic arrays and structs. If you enable the
    new ``ABIEncoderV2`` feature by adding ``pragma experimental
    ABIEncoderV2;`` to your source file then more types are available, but
    ``mapping`` types are still limited to inside a single contract and you
    cannot transfer them.

.. _multi-return:

Returning Multiple Values
-------------------------

When a function has multiple return types, the statement ``return (v0, v1, ..., vn)`` can be used to return multiple values.
The number of components must be the same as the number of return variables
and their types have to match, potentially after an :ref:`implicit conversion <types-conversion-elementary-types>`.

.. index:: ! view function, function;view

.. _view-functions:

View Functions
==============

Functions can be declared ``view`` in which case they promise not to modify the state.

.. note::
  If the compiler's EVM target is Byzantium or newer (default) the opcode
  ``STATICCALL`` is used when ``view`` functions are called, which enforces the state
  to stay unmodified as part of the EVM execution. For library ``view`` functions
  ``DELEGATECALL`` is used, because there is no combined ``DELEGATECALL`` and ``STATICCALL``.
  This means library ``view`` functions do not have run-time checks that prevent state
  modifications. This should not impact security negatively because library code is
  usually known at compile-time and the static checker performs compile-time checks.

The following statements are considered modifying the state:

#. Writing to state variables.
#. :ref:`Emitting events <events>`.
#. :ref:`Creating other contracts <creating-contracts>`.
#. Using ``selfdestruct``.
#. Sending Ether via calls.
#. Calling any function not marked ``view`` or ``pure``.
#. Using low-level calls.
#. Using inline assembly that contains certain opcodes.

::

    // SPDX-License-Identifier: GPL-3.0
    pragma solidity >=0.5.0 <0.8.0;

    contract C {
        function f(uint a, uint b) public view returns (uint) {
            return a * (b + 42) + block.timestamp;
        }
    }

.. note::
  ``constant`` on functions used to be an alias to ``view``, but this was dropped in version 0.5.0.

.. note::
  Getter methods are automatically marked ``view``.

.. note::
  Prior to version 0.5.0, the compiler did not use the ``STATICCALL`` opcode
  for ``view`` functions.
  This enabled state modifications in ``view`` functions through the use of
  invalid explicit type conversions.
  By using  ``STATICCALL`` for ``view`` functions, modifications to the
  state are prevented on the level of the EVM.

.. index:: ! pure function, function;pure

.. _pure-functions:

Pure Functions
==============

Functions can be declared ``pure`` in which case they promise not to read from or modify the state.

.. note::
  If the compiler's EVM target is Byzantium or newer (default) the opcode ``STATICCALL`` is used,
  which does not guarantee that the state is not read, but at least that it is not modified.

In addition to the list of state modifying statements explained above, the following are considered reading from the state:

#. Reading from state variables.
#. Accessing ``address(this).balance`` or ``<address>.balance``.
#. Accessing any of the members of ``block``, ``tx``, ``msg`` (with the exception of ``msg.sig`` and ``msg.data``).
#. Calling any function not marked ``pure``.
#. Using inline assembly that contains certain opcodes.

::

    // SPDX-License-Identifier: GPL-3.0
    pragma solidity >=0.5.0 <0.8.0;

    contract C {
        function f(uint a, uint b) public pure returns (uint) {
            return a * (b + 42);
        }
    }

Pure functions are able to use the ``revert()`` and ``require()`` functions to revert
potential state changes when an :ref:`error occurs <assert-and-require>`.

Reverting a state change is not considered a "state modification", as only changes to the
state made previously in code that did not have the ``view`` or ``pure`` restriction
are reverted and that code has the option to catch the ``revert`` and not pass it on.

This behaviour is also in line with the ``STATICCALL`` opcode.

.. warning::
  It is not possible to prevent functions from reading the state at the level
  of the EVM, it is only possible to prevent them from writing to the state
  (i.e. only ``view`` can be enforced at the EVM level, ``pure`` can not).

.. note::
  Prior to version 0.5.0, the compiler did not use the ``STATICCALL`` opcode
  for ``pure`` functions.
  This enabled state modifications in ``pure`` functions through the use of
  invalid explicit type conversions.
  By using  ``STATICCALL`` for ``pure`` functions, modifications to the
  state are prevented on the level of the EVM.

.. note::
  Prior to version 0.4.17 the compiler did not enforce that ``pure`` is not reading the state.
  It is a compile-time type check, which can be circumvented doing invalid explicit conversions
  between contract types, because the compiler can verify that the type of the contract does
  not do state-changing operations, but it cannot check that the contract that will be called
  at runtime is actually of that type.

.. index:: ! receive ether function, function;receive ! receive

.. _receive-ether-function:

Receive Ether Function
======================

A contract can have at most one ``receive`` function, declared using
``receive() external payable { ... }``
(without the ``function`` keyword).
This function cannot have arguments, cannot return anything and must have
``external`` visibility and ``payable`` state mutability. It is executed on a
call to the contract with empty calldata. This is the function that is executed
on plain Ether transfers (e.g. via ``.send()`` or ``.transfer()``). If no such
function exists, but a payable :ref:`fallback function <fallback-function>`
exists, the fallback function will be called on a plain Ether transfer. If
neither a receive Ether nor a payable fallback function is present, the
contract cannot receive Ether through regular transactions and throws an
exception.

In the worst case, the ``receive`` function can only rely on 2300 gas being
available (for example when ``send`` or ``transfer`` is used), leaving little
room to perform other operations except basic logging. The following operations
will consume more gas than the 2300 gas stipend:

- Writing to storage
- Creating a contract
- Calling an external function which consumes a large amount of gas
- Sending Ether

.. warning::
    Contracts that receive Ether directly (without a function call, i.e. using ``send`` or ``transfer``)
    but do not define a receive Ether function or a payable fallback function
    throw an exception, sending back the Ether (this was different
    before Solidity v0.4.0). So if you want your contract to receive Ether,
    you have to implement a receive Ether function (using payable fallback functions for receiving Ether is
    not recommended, since it would not fail on interface confusions).


.. warning::
    A contract without a receive Ether function can receive Ether as a
    recipient of a *coinbase transaction* (aka *miner block reward*)
    or as a destination of a ``selfdestruct``.

    A contract cannot react to such Ether transfers and thus also
    cannot reject them. This is a design choice of the EVM and
    Solidity cannot work around it.

    It also means that ``address(this).balance`` can be higher
    than the sum of some manual accounting implemented in a
    contract (i.e. having a counter updated in the receive Ether function).

Below you can see an example of a Sink contract that uses function ``receive``.

::

    // SPDX-License-Identifier: GPL-3.0
    pragma solidity >=0.6.0 <0.8.0;

    // This contract keeps all Ether sent to it with no way
    // to get it back.
    contract Sink {
        event Received(address, uint);
        receive() external payable {
            emit Received(msg.sender, msg.value);
        }
    }

.. index:: ! fallback function, function;fallback

.. _fallback-function:

Fallback Function
=================

A contract can have at most one ``fallback`` function, declared using ``fallback () external [payable]``
(without the ``function`` keyword).
This function cannot have arguments, cannot return anything and must have ``external`` visibility.
It is executed on a call to the contract if none of the other
functions match the given function signature, or if no data was supplied at
all and there is no :ref:`receive Ether function <receive-ether-function>`.
The fallback function always receives data, but in order to also receive Ether
it must be marked ``payable``.

In the worst case, if a payable fallback function is also used in
place of a receive function, it can only rely on 2300 gas being
available (see :ref:`receive Ether function <receive-ether-function>`
for a brief description of the implications of this).

Like any function, the fallback function can execute complex
operations as long as there is enough gas passed on to it.

.. warning::
    A ``payable`` fallback function is also executed for
    plain Ether transfers, if no :ref:`receive Ether function <receive-ether-function>`
    is present. It is recommended to always define a receive Ether
    function as well, if you define a payable fallback function
    to distinguish Ether transfers from interface confusions.

.. note::
    Even though the fallback function cannot have arguments, one can still use ``msg.data`` to retrieve
    any payload supplied with the call.
    After having checked the first four bytes of ``msg.data``,
    you can use ``abi.decode`` together with the array slice syntax to
    decode ABI-encoded data:
    ``(c, d) = abi.decode(msg.data[4:], (uint256, uint256));``
    Note that this should only be used as a last resort and
    proper functions should be used instead.


::

    // SPDX-License-Identifier: GPL-3.0
    pragma solidity >=0.6.2 <0.8.0;

    contract Test {
        // This function is called for all messages sent to
        // this contract (there is no other function).
        // Sending Ether to this contract will cause an exception,
        // because the fallback function does not have the `payable`
        // modifier.
        fallback() external { x = 1; }
        uint x;
    }

    contract TestPayable {
        // This function is called for all messages sent to
        // this contract, except plain Ether transfers
        // (there is no other function except the receive function).
        // Any call with non-empty calldata to this contract will execute
        // the fallback function (even if Ether is sent along with the call).
        fallback() external payable { x = 1; y = msg.value; }

        // This function is called for plain Ether transfers, i.e.
        // for every call with empty calldata.
        receive() external payable { x = 2; y = msg.value; }
        uint x;
        uint y;
    }

    contract Caller {
        function callTest(Test test) public returns (bool) {
            (bool success,) = address(test).call(abi.encodeWithSignature("nonExistingFunction()"));
            require(success);
            // results in test.x becoming == 1.

            // address(test) will not allow to call ``send`` directly, since ``test`` has no payable
            // fallback function.
            // It has to be converted to the ``address payable`` type to even allow calling ``send`` on it.
            address payable testPayable = payable(address(test));

            // If someone sends Ether to that contract,
            // the transfer will fail, i.e. this returns false here.
            return testPayable.send(2 ether);
        }

        function callTestPayable(TestPayable test) public returns (bool) {
            (bool success,) = address(test).call(abi.encodeWithSignature("nonExistingFunction()"));
            require(success);
            // results in test.x becoming == 1 and test.y becoming 0.
            (success,) = address(test).call{value: 1}(abi.encodeWithSignature("nonExistingFunction()"));
            require(success);
            // results in test.x becoming == 1 and test.y becoming 1.

            // If someone sends Ether to that contract, the receive function in TestPayable will be called.
            require(address(test).send(2 ether));
            // results in test.x becoming == 2 and test.y becoming 2 ether.
        }
    }

.. index:: ! overload

.. _overload-function:

Function Overloading
====================

A contract can have multiple functions of the same name but with different parameter
types.
This process is called "overloading" and also applies to inherited functions.
The following example shows overloading of the function
``f`` in the scope of contract ``A``.

::

    // SPDX-License-Identifier: GPL-3.0
    pragma solidity >=0.4.16 <0.8.0;

    contract A {
        function f(uint _in) public pure returns (uint out) {
            out = _in;
        }

        function f(uint _in, bool _really) public pure returns (uint out) {
            if (_really)
                out = _in;
        }
    }

Overloaded functions are also present in the external interface. It is an error if two
externally visible functions differ by their Solidity types but not by their external types.

::

    // SPDX-License-Identifier: GPL-3.0
    pragma solidity >=0.4.16 <0.8.0;

    // This will not compile
    contract A {
        function f(B _in) public pure returns (B out) {
            out = _in;
        }

        function f(address _in) public pure returns (address out) {
            out = _in;
        }
    }

    contract B {
    }


Both ``f`` function overloads above end up accepting the address type for the ABI although
they are considered different inside Solidity.

Overload resolution and Argument matching
-----------------------------------------

Overloaded functions are selected by matching the function declarations in the current scope
to the arguments supplied in the function call. Functions are selected as overload candidates
if all arguments can be implicitly converted to the expected types. If there is not exactly one
candidate, resolution fails.

.. note::
    Return parameters are not taken into account for overload resolution.

::

    // SPDX-License-Identifier: GPL-3.0
    pragma solidity >=0.4.16 <0.8.0;

    contract A {
        function f(uint8 _in) public pure returns (uint8 out) {
            out = _in;
        }

        function f(uint256 _in) public pure returns (uint256 out) {
            out = _in;
        }
    }

Calling ``f(50)`` would create a type error since ``50`` can be implicitly converted both to ``uint8``
and ``uint256`` types. On another hand ``f(256)`` would resolve to ``f(uint256)`` overload as ``256`` cannot be implicitly
converted to ``uint8``.
