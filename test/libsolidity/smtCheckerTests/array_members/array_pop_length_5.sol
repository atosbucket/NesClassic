pragma experimental SMTChecker;

contract C {
	uint[] a;
	function g() internal {
		a.push();
	}
	function f() public {
		a.pop();
		g();
	}
}
// ----
// Warning 2529: (122-129): CHC: Empty array "pop" detected here.
