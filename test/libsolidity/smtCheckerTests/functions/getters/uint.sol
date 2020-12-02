pragma experimental SMTChecker;

contract C {
	uint public x;

	function f() public view {
		uint y = this.x();
		assert(y == x); // should hold
		assert(y == 1); // should fail
	}
}
// ----
// Warning 6328: (147-161): CHC: Assertion violation happens here.
