pragma experimental SMTChecker;
contract BaseBase {
	uint x;
	function init(uint a, uint b) public virtual {
		x = a;
	}
}
contract Base is BaseBase {
	function init(uint a, uint b) public override {
	}
}
contract Child is Base {
	function bInit(uint c, uint d) public {
		BaseBase.init(c, d);
		assert(x == c);
		assert(x == d); // should fail
	}
}
// ----
// Warning 5667: (84-90): Unused function parameter. Remove or comment out the variable name to silence this warning.
// Warning 6328: (314-328): CHC: Assertion violation happens here.\nCounterexample:\nx = 0\nc = 0\nd = 1\n\nTransaction trace:\nChild.constructor()\nState: x = 0\nChild.bInit(0, 1)\n    BaseBase.init(0, 1) -- internal call
