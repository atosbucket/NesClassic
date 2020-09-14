pragma experimental SMTChecker;
pragma experimental ABIEncoderV2;

contract C {
	struct S {
		uint x;
		uint[] a;
	}
	function f(S memory s1, S memory s2) public pure {
		delete s1;
		assert(s1.x == s2.x);
		assert(s1.a.length == s2.a.length);
		assert(s1.a.length == 0);
	}
}
// ----
// Warning 6328: (184-204): Assertion violation happens here.
// Warning 6328: (208-242): Assertion violation happens here.
