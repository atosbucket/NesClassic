pragma experimental SMTChecker;

contract C {
	struct S {
		uint8 x;
	}

	S s;

	constructor() {
		s.x = 254;
	}

	function inc_pre() public {
		++s.x;
	}

	function check() view public {
		uint y = s.x;
		assert(y < 256);
	}
}
// ----
// Warning 4984: (145-150): CHC: Overflow (resulting value larger than 255) happens here.
