pragma experimental SMTChecker;

contract A {
	int x;
	constructor (int a) { x = a;}
}

contract B is A {
	int y;
	constructor(int a) A(-a) {
		if (a > 0) {
			y = 2;
		}
		else {
			y = 4;
		}
	}
}

contract C is B {
	constructor(int a) B(a) {
		assert(y != 3); // should hold
		assert(y == 4); // should fail
		if (a > 0) {
			assert(x < 0 && y == 2); // should hold
			assert(x < 0 && y == 4); // should fail
		}
		else {
			assert(x >= 0 && y == 4); // should hold
			assert(x >= 0 && y == 2); // should fail
			assert(x > 0); // should fail
		}
	}
}
// ----
// Warning 6328: (280-294): CHC: Assertion violation happens here.
// Warning 6328: (372-395): CHC: Assertion violation happens here.
// Warning 6328: (472-496): CHC: Assertion violation happens here.
// Warning 6328: (516-529): CHC: Assertion violation happens here.
