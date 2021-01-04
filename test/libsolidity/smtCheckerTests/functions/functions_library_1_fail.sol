pragma experimental SMTChecker;

library L
{
	function add(uint x, uint y) internal pure returns (uint) {
		require(x < 1000);
		require(y < 1000);
		return x + y;
	}
}

contract C
{
	function f(uint x) public pure {
		uint y = L.add(x, 999);
		assert(y < 1000);
	}
}
// ----
// Warning 6328: (245-261): CHC: Assertion violation happens here.\nCounterexample:\n\nx = 1\n\n\nTransaction trace:\nC.constructor()\nC.f(1)
