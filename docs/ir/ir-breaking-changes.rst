********************************
Solidity IR-based Codegen Changes
********************************

This section highlights the main differences between the old and the IR-based codegen,
along with the reasoning behind the changes and how to update affected code.

Semantic Only Changes
=====================

This section lists the changes that are semantic-only, thus potentially
hiding new and different behavior in existing code.

 * When storage structs are deleted, every storage slot that contains a member of the struct is set to zero entirely. Formally, padding space was left untouched.
Consequently, if the padding space within a struct is used to store data (e.g. in the context of a contract upgrade), you have to be aware that ``delete`` will now also clear the added member (while it wouldn't have been cleared in the past).

.. code-block:: solidity

    // SPDX-License-Identifier: GPL-3.0
    pragma solidity >0.7.0;

    contract C {
        struct S {
            uint64 y;
            uint64 z;
        }
        S s;
        function f() public {
            // ...
            delete s;
            // s occupies only first 16 bytes of the 32 bytes slot
            // delete will write zero to the full slot
        }
    }

We have the same behavior for implicit delete, for example when array of structs is shortened.

 * Function modifiers are implemented in a slightly different way regarding function parameters.
   This especially has an effect if the placeholder ``_;`` is evaluated multiple times in a modifier.
   In the old code generator, each function parameter has a fixed slot on the stack. If the function
   is run multiple times because ``_;`` is used multiple times or used in a loop, then a change to the
   function parameter's value is visible in the next execution of the function.
   The new code generator implements modifiers using actual functions and passes function parameters on.
   This means that multiple executions of a function will get the same values for the parameters.

.. code-block:: solidity

    // SPDX-License-Identifier: GPL-3.0
    pragma solidity >=0.7.0;
    contract C {
        function f(uint _a) public pure mod() returns (uint _r) {
            _r = _a++;
        }
        modifier mod() { _; _; }
    }

If you execute ``f(0)`` in the old code generator, it will return ``2``, while
it will return ``1`` when using the new code generator.

 * The order of contract initialization has changed in case of inheritance.

The order used to be:
 - All state variables are zero-initialized at the beginning.
 - Evaluate base constructor arguments from most derived to most base contract.
 - Initialize all state variables in the whole inheritance hierarchy from most base to most derived.
 - Run the constructor, if present, for all contracts in the linearized hierarchy from most base to most derived.

New order:
 - All state variables are zero-initialized at the beginning.
 - Evaluate base constructor arguments from most derived to most base contract.
 - For every contract in order from most base to most derived in the linearized hierarchy execute:
     1. If present at declaration, initial values are assigned to state variables.
     2. Constructor, if present.

This causes differences in some contracts, for example:

.. code-block:: solidity

    // SPDX-License-Identifier: GPL-3.0
    pragma solidity >0.7.0;

    contract A {
        uint x;
        constructor() {
            x = 42;
        }
        function f() public view returns(uint256) {
            return x;
        }
    }
    contract B is A {
        uint public y = f();
    }

Previously, ``y`` would be set to 0. This is due to the fact that we would first initialize state variables: First, ``x`` is set to 0, and when initializing ``y``, ``f()`` would return 0 causing ``y`` to be 0 as well.
With the new rules, ``y`` will be set to 42. We first initialize ``x`` to 0, then call A's constructor which sets ``x`` to 42. Finally, when initializing ``y``, ``f()`` returns 42 causing ``y`` to be 42.

 * Copying ``bytes`` arrays from memory to storage is implemented in a different way. The old code generator always copies full words, while the new one cuts the byte array after its end. The old behaviour can lead to dirty data being copied after the end of the array (but still in the same storage slot).
This causes differences in some contracts, for example:

.. code-block:: solidity

    // SPDX-License-Identifier: GPL-3.0
    pragma solidity >0.8.0;

    contract C {
        bytes x;
        function f() public returns (uint _r) {
            bytes memory m = "tmp";
            assembly {
                mstore(m, 8)
                mstore(add(m, 32), "deadbeef15dead")
            }
            x = m;
            assembly {
                _r := sload(x.slot)
            }
        }
    }

Previously ``f()`` would return ``0x6465616462656566313564656164000000000000000000000000000000000010`` (it has correct length, and correct first 8 elements, but then it contains dirty data which was set via assembly).
Now it is returning ``0x6465616462656566000000000000000000000000000000000000000000000010`` (it has correct length, and correct elements, but does not contain superfluous data).

.. index:: ! evaluation order; expression

* For the old code generator, the evaluation order of expressions is unspecified.
  For the new code generator, we try to evaluate in source order (left to right), but do not guarantee it.
  This can lead to semantic differences.

For example:

.. code-block:: solidity

    // SPDX-License-Identifier: GPL-3.0
    pragma solidity >0.8.0;
    contract C {
        function preincr_u8(uint8 _a) public pure returns (uint8) {
            return ++_a + _a;
        }
    }

The function ``preincr_u8(1)`` returns the following values:
- Old code generator: 3 (``1 + 2``) but the return value is unspecified in general
- New code generator: 4 (``2 + 2``) but the return value is not guaranteed

.. index:: ! evaluation order; function arguments

On the other hand, function argument expressions are evaluated in the same order by both code generators with the exception of the global functions ``addmod`` and ``mulmod``.
For example:

.. code-block:: solidity

    // SPDX-License-Identifier: GPL-3.0
    pragma solidity >0.8.0;
    contract C {
        function add(uint8 _a, uint8 _b) public pure returns (uint8) {
            return _a + _b;
        }
        function g(uint8 _a, uint8 _b) public pure returns (uint8) {
            return add(++_a + ++_b, _a + _b);
        }
    }

The function ``g(1, 2)`` returns the following values:
- Old code generator: ``10`` (``add(2 + 3, 2 + 3)``) but the return value is unspecified in general
- New code generator: ``10`` but the return value is not guaranteed

The arguments to the global functions ``addmod`` and ``mulmod`` are evaluated right-to-left by the old code generator
and left-to-right by the new code generator.
For example:

::
    // SPDX-License-Identifier: GPL-3.0
    pragma solidity >0.8.0;
    contract C {
        function f() public pure returns (uint256 aMod, uint256 mMod) {
            uint256 x = 3;
            // Old code gen: add/mulmod(5, 4, 3)
            // New code gen: add/mulmod(4, 5, 5)
            aMod = addmod(++x, ++x, x);
            mMod = mulmod(++x, ++x, x);
        }
    }

The function ``f()`` returns the following values:
- Old code generator: ``aMod = 0`` and ``mMod = 2``
- New code generator: ``aMod = 4`` and ``mMod = 0``


Internals
=========

Internal function pointers
--------------------------

.. index:: function pointers

The old code generator uses code offsets or tags for values of internal function pointers. This is especially complicated since
these offsets are different at construction time and after deployment and the values can cross this border via storage.
Because of that, both offsets are encoded at construction time into the same value (into different bytes).

In the new code generator, function pointers use the AST IDs of the functions as values. Since calls via jumps are not possible,
calls through function pointers always have to use an internal dispatch function that uses the ``switch`` statement to select
the right function.

The ID ``0`` is reserved for uninitialized function pointers which then cause a panic in the disptach function when called.

In the old code generator, internal function pointers are initialized with a special function that always causes a panic.
This causes a storage write at construction time for internal function pointers in storage.

Cleanup
-------

.. index:: cleanup, dirty bits

The old code generator only performs cleanup before an operation whose result could be affected by the values of the dirty bits.
The new code generator performs cleanup after any operation that can result in dirty bits.

For example:

.. code-block:: solidity

    // SPDX-License-Identifier: GPL-3.0
    pragma solidity >0.8.0;
    contract C {
        function f(uint8 _a) public pure returns (uint _r1, uint _r2)
        {
            _a = ~_a;
            assembly {
                _r1 := _a
            }
            _r2 = _a;
        }
    }

The function ``f(1)`` returns the following values:
- Old code generator: (``fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffe``, ``00000000000000000000000000000000000000000000000000000000000000fe``)
- New code generator: (``00000000000000000000000000000000000000000000000000000000000000fe``, ``00000000000000000000000000000000000000000000000000000000000000fe``)

Note that, unlike the new code generator, the old code generator does not perform a cleanup after the bit-not assignment (``_a = ~_a``).
This results in different values being assigned (within the inline assembly block) to return value ``_r1`` between the old and new code generators.
However, both code generators perform a cleanup before the new value of ``_a`` is assigned to ``_r2``.
