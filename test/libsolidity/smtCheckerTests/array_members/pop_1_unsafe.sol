pragma experimental SMTChecker;

contract C {
	uint[] a;
	function f() public {
		a.pop();
	}
}
// ----
// Warning 2529: (82-89): CHC: Empty array "pop" happens here.\nCounterexample:\na = []\n\n\n\nTransaction trace:\nC.constructor()\nState: a = []\nC.f()
