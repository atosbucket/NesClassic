pragma experimental SMTChecker;

contract A {
	uint x;
	function f() internal {
		assert(x == 2);
		--x;
	}
}

contract C is A {
	constructor() {
		assert(x == 1);
		++x;
		f();
		assert(x == 1);
	}
}
// ----
// Warning 6328: (82-96): CHC: Assertion violation happens here.
// Warning 6328: (148-162): CHC: Assertion violation happens here.
// Warning 6328: (180-194): CHC: Assertion violation happens here.
// Warning 4144: (100-103): BMC: Underflow (resulting value less than 0) happens here.
// Warning 4144: (100-103): BMC: Underflow (resulting value less than 0) happens here.
