pragma experimental SMTChecker;

contract C
{
	function f() internal pure returns (uint, uint) {
		return (2, 3);
	}
	function g() public pure {
		uint x;
		uint y;
		(x,) = f();
		assert(x == 2);
		assert(y == 4);
	}
}
// ----
// Warning 6328: (199-213): CHC: Assertion violation happens here.
