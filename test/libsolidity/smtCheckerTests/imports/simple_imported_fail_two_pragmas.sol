==== Source: A.sol ====
pragma experimental SMTChecker;
contract A {
	function f(uint x) public pure {
		assert(x > 0);
	}
}
==== Source: B.sol ====
import "A.sol";
pragma experimental SMTChecker;
contract C is A {}
// ----
// Warning 6328: (A.sol:81-94): CHC: Assertion violation happens here.\nCounterexample:\n\nx = 0\n\n\nTransaction trace:\nA.constructor()\nA.f(0)
// Warning 6328: (A.sol:81-94): CHC: Assertion violation happens here.\nCounterexample:\n\nx = 0\n\n\nTransaction trace:\nA.constructor()\nA.f(0)
