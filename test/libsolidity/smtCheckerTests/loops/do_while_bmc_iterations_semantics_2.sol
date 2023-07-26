contract C {
	function f() public pure {
		uint x = 0;
		int y = 0;
		do {
			if (x >= 3)
				y = 1;
			++x;
		} while (x < 3 || y == 1);
        // BMC loop iteration setting is just enough to leave the loop
		assert(x == 3); // should hold
		assert(y == 1); // should fail, when x == 3 loop is completed
	}
}
// ====
// SMTEngine: bmc
// SMTSolvers: z3
// BMCLoopIterations: 4
// ----
// Warning 4661: (244-258): BMC: Assertion violation happens here.
// Info 6002: BMC: 2 verification condition(s) proved safe! Enable the model checker option "show proved safe" to see all of them.
