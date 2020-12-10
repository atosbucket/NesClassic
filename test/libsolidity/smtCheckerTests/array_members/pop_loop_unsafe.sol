pragma experimental SMTChecker;

contract C {
	uint[] a;
	function f(uint l) public {
		for (uint i = 0; i < l; ++i) {
			a.push();
			a.pop();
		}
		a.pop();
	}
}
// ----
// Warning 2529: (150-157): CHC: Empty array "pop" happens here.\nCounterexample:\na = []\nl = 0\n\n\nTransaction trace:\nconstructor()\nState: a = []\nf(0)
