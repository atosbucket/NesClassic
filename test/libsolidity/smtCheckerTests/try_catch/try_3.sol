pragma experimental SMTChecker;
contract C {
	int x;

	function g(int y) public pure returns (int) {
		return y;
	}

	function postinc() internal returns (int) {
		x += 1;
		return x;
	}

	function f() public {
		x = 0;
		try this.g(postinc()) {
			assert(x == 1); // should hold
		} catch (bytes memory s) {
			assert(x == 0); // should fail - state is reverted to the state after postinc(), but before the call to this.g()
		}
	}
}
// ----
// Warning 5667: (291-305): Unused try/catch parameter. Remove or comment out the variable name to silence this warning.
// Warning 6328: (312-326): CHC: Assertion violation happens here.\nCounterexample:\nx = 1\n\nTransaction trace:\nC.constructor()\nState: x = 0\nC.f()\n    C.postinc() -- internal call
