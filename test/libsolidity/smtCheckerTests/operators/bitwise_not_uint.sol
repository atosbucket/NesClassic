pragma experimental SMTChecker;

contract C {
	function f() public pure {
		uint8 x = 0xff;
		assert(~x == 0x00);
		uint16 y = 0xff00;
		assert(~y == 0xff);
		assert(~y == 0xffff);
		assert(~y == 0x0000);
	}
}
// ----
// Warning 6328: (159-179): CHC: Assertion violation happens here.\nCounterexample:\n\nx = 255\ny = 65280\n\nTransaction trace:\nC.constructor()\nC.f()
// Warning 6328: (183-203): CHC: Assertion violation happens here.\nCounterexample:\n\nx = 255\ny = 65280\n\nTransaction trace:\nC.constructor()\nC.f()
