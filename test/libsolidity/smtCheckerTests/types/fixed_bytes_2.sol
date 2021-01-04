pragma experimental SMTChecker;

contract C
{
	bytes32 x;
	function f(bytes8 y) public view {
		assert(x == g());
		assert(x != y);
	}
	function g() public view returns (bytes32) {
		return x;
	}
}
// ----
// Warning 6328: (116-130): CHC: Assertion violation happens here.\nCounterexample:\nx = 0\ny = 0\n\n\nTransaction trace:\nC.constructor()\nState: x = 0\nC.f(0)
