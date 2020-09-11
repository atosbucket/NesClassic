pragma experimental SMTChecker;

contract A {
}

contract C {
	function f() public pure {
		assert(bytes(type(C).name).length != 0);
		assert(type(A).creationCode.length != 0);
		assert(type(A).runtimeCode.length != 0);
	}
}
// ----
// Warning 6328: (92-131): Assertion violation happens here.
// Warning 6328: (135-175): Assertion violation happens here.
// Warning 6328: (179-218): Assertion violation happens here.
// Warning 7507: (105-117): Assertion checker does not yet support this expression.
// Warning 7507: (142-162): Assertion checker does not yet support this expression.
// Warning 7507: (186-205): Assertion checker does not yet support this expression.
