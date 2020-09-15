pragma experimental SMTChecker;

contract C {
	function f() pure public {
		revert();
		// This is not reachable.
		assert(false);
	}

	function g() pure public {
		revert("revert message");
		// This is not reachable.
		assert(false);
	}

	function h(bool b) pure public {
		if (b)
			revert();
		assert(!b);
	}

	// Check that arguments are evaluated.
	bool x = false;
	function m() view internal returns (string memory) {
		assert(x != true);
	}
	function i() public {
		x = true;
		revert(m());
	}
}
// ----
// Warning 5740: (116-129): Unreachable code.
// Warning 5740: (221-234): Unreachable code.
// Warning 6328: (427-444): Assertion violation happens here.
