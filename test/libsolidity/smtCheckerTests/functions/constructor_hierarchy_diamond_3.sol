pragma experimental SMTChecker;
contract C {
	uint a;
	constructor(uint x) {
		a = x;
	}
}

abstract contract B1 is C {
	uint b1;
	constructor(uint x) {
		b1 = x + a;
	}
}

contract B2 is C {
	uint b2;
	constructor(uint x) C(x + 2) {
		b2 = x + a;
	}
}

contract A is B2, B1 {
	constructor(uint x) B2(x) B1(x) {
		assert(b1 == b2);
		assert(b1 != b2);
	}
}
// ----
// Warning 4984: (160-165): Overflow (resulting value larger than 2**256 - 1) happens here.
// Warning 4984: (225-230): Overflow (resulting value larger than 2**256 - 1) happens here.
// Warning 4984: (241-246): Overflow (resulting value larger than 2**256 - 1) happens here.
// Warning 6328: (334-350): Assertion violation happens here.
