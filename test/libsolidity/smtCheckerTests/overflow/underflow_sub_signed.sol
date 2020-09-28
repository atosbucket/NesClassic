pragma experimental SMTChecker;

contract C
{
	function f(int8 x) public pure returns (int8) {
		x = -2;
		int8 y = x - 127;
		assert(y == 127);
		x = -128;
		y = x - 127;
		assert(y == 1);
		x = 127;
		y = x - (-127);
		assert(y == -2);
		return y;
	}
}
// ----
// Warning 3944: (116-123): CHC: Underflow (resulting value less than -128) happens here.
// Warning 3944: (163-170): CHC: Underflow (resulting value less than -128) happens here.
// Warning 4984: (207-217): CHC: Overflow (resulting value larger than 127) happens here.
