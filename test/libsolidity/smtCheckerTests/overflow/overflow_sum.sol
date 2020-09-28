pragma experimental SMTChecker;

contract C
{
	function f(uint8 x) public pure returns (uint8) {
		uint8 y = x + 255;
		require(y >= x);
		x = 255;
		y = x + 1;
		assert(y == 0);
		y = x + 255;
		assert(y == 254);
		return y;
	}
}
// ----
// Warning 4984: (154-159): CHC: Overflow (resulting value larger than 255) happens here.
// Warning 4984: (185-192): CHC: Overflow (resulting value larger than 255) happens here.
