pragma experimental SMTChecker;

abstract contract D {
	function d() external virtual;
}

contract C {
	uint x;
	D d;

	function inc() public {
		++x;
	}

	function f() public {
		d.d();
		assert(x < 10);
	}
}
// ====
// SMTIgnoreCex: yes
// ----
// Warning 4984: (146-149): CHC: Overflow (resulting value larger than 2**256 - 1) might happen here.
// Warning 6328: (189-203): CHC: Assertion violation happens here.
// Warning 2661: (146-149): BMC: Overflow (resulting value larger than 2**256 - 1) happens here.
