pragma experimental SMTChecker;
contract C {
	function abiencodePackedSimple(bool t, uint x, uint y, uint z, uint[] memory a, uint[] memory b) public pure {
		require(x == y);
		bytes memory b1 = abi.encodePacked(x, z, a);
		bytes memory b2 = abi.encodePacked(y, z, a);
		assert(b1.length == b2.length);

		bytes memory b3 = abi.encodePacked(y, z, b);
		assert(b1.length == b3.length); // should fail

		bytes memory b4 = abi.encodePacked(t, z, a);
		assert(b1.length == b4.length); // should fail

		bytes memory b5 = abi.encodePacked(y, y, y, y, a, a, a);
		assert(b1.length != b5.length); // should fail
		assert(b1.length == b5.length); // should fail

		bytes memory b6 = abi.encode(x, z, a);
		assert(b1.length == b6.length); // should fail
	}
}
// ----
// Warning 6328: (354-384): CHC: Assertion violation happens here.
// Warning 1218: (451-481): CHC: Error trying to invoke SMT solver.
// Warning 6328: (451-481): CHC: Assertion violation might happen here.
// Warning 1218: (560-590): CHC: Error trying to invoke SMT solver.
// Warning 6328: (560-590): CHC: Assertion violation might happen here.
// Warning 1218: (609-639): CHC: Error trying to invoke SMT solver.
// Warning 6328: (609-639): CHC: Assertion violation might happen here.
// Warning 6328: (700-730): CHC: Assertion violation happens here.
// Warning 4661: (451-481): BMC: Assertion violation happens here.
// Warning 4661: (560-590): BMC: Assertion violation happens here.
// Warning 4661: (609-639): BMC: Assertion violation happens here.
