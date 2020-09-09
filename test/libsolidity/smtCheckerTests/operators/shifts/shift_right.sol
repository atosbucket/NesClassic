pragma experimental SMTChecker;

contract C {
    function f(uint256 a, uint256 b) internal pure returns (uint256) {
        return a >> b;
    }
	function t() public pure {
		assert(f(0x4266, 0) == 0x4266);
		// Fails because the above is true.
		assert(f(0x4266, 0) == 0x426);

		assert(f(0x4266, 0x8) == 0x42);
		// Fails because the above is true.
		assert(f(0x4266, 0x8) == 0x420);

		assert(f(0x4266, 0x11) == 0);
		// Fails because the above is true.
		assert(f(0x4266, 0x11) == 1);

		assert(f(57896044618658097711785492504343953926634992332820282019728792003956564819968, 5) == 1809251394333065553493296640760748560207343510400633813116524750123642650624);
		// Fails because the above is true.
		assert(f(57896044618658097711785492504343953926634992332820282019728792003956564819968, 5) == 0);
	}
}
// ----
// Warning 6328: (248-277): Assertion violation happens here.
// Warning 6328: (354-385): Assertion violation happens here.
// Warning 6328: (460-488): Assertion violation happens here.
// Warning 6328: (706-802): Assertion violation happens here.
