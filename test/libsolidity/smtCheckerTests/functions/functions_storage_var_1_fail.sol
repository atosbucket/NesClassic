pragma experimental SMTChecker;
contract C
{
	uint a;
	function f(uint x) public {
		uint y;
		a = (y = x);
	}
	function g() public {
		f(0);
		assert(a > 0);
	}
}

// ----
// Warning 6328: (144-157): CHC: Assertion violation happens here.
