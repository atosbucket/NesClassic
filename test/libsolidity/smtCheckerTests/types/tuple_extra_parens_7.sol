pragma experimental SMTChecker;
contract C {
	function g() internal pure returns (uint, uint) {
		return (2, 3);
	}
	function f() public {
		(address(1).call(""));
		(uint x, uint y) = ((g()));
		assert(x == 2);
		assert(y == 3);
	}
}
// ----
// Warning 5084: (142-152): Type conversion is not yet fully supported and might yield false positives.
