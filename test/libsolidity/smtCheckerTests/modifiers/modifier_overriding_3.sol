pragma experimental SMTChecker;

abstract contract A {
	bool s;

	function f() public view mod {
		assert(s); // holds for B
		assert(!s); // fails for B
	}
	modifier mod() virtual;
}

contract B is A {
	modifier mod() virtual override {
		bool x = true;
		s = x;
		_;
	}
}
// ----
// Warning 5740: (95-156): Unreachable code.
// Warning 6328: (127-137): CHC: Assertion violation happens here.\nCounterexample:\ns = true\n\n\n\nTransaction trace:\nB.constructor()\nState: s = false\nA.f()
