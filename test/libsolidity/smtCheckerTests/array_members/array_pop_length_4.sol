pragma experimental SMTChecker;

contract C {
	uint[] a;
	function f() public {
		a.length;
		a.pop();
	}
}
// ----
// Warning 2529: (94-101): CHC: Empty array "pop" detected here.
