pragma experimental SMTChecker;

contract C {
	uint[] a;
	function f() public {
		a.pop();
		a.push();
	}
}
// ----
// Warning 2529: (82-89): CHC: Empty array "pop" detected here.
