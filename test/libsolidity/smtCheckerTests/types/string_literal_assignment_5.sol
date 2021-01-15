pragma experimental SMTChecker;

contract C {
	function g() internal pure returns (bytes32, bytes16) {
		return ("test", "testz");
	}

	function f(bytes32 _x) public pure {
		require(_x == "test");
		(bytes32 y, bytes16 z) = g();
		assert(_x == y);
		assert(_x == z);
	}
}
// ----
// Warning 6328: (251-266): CHC: Assertion violation happens here.\nCounterexample:\n\n_x = 52647538817385212172903286807934654968315727694643370704309751478220717293568\n\nTransaction trace:\nC.constructor()\nC.f(52647538817385212172903286807934654968315727694643370704309751478220717293568)\n    C.g() -- internal call
