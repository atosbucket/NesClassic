pragma experimental SMTChecker;

contract C
{
	function f(uint8 x) public pure returns (uint8) {
		x = 100;
		uint8 y = x * 3;
		assert(y == 44);
		x = 128;
		y = x * 4;
		assert(y == 0);
		return y;
	}
}
// ----
// Warning 4984: (120-125): CHC: Overflow (resulting value larger than 255) happens here.\nCounterexample:\n\nx = 100\n = 0\n\nTransaction trace:\nC.constructor()\nC.f(0)
