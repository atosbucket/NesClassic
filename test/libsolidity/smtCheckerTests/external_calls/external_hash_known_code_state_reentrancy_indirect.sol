pragma experimental SMTChecker;

contract Other {
	C c;
	function h() public {
		c.setOwner(address(0));
	}
}

contract State {
	uint x;
	Other o;
	C c;
	function f() public returns (uint) {
		o.h();
		return c.g();
	}
}

contract C {
	address owner;
	uint y;
	State s;

	constructor() public {
		owner = msg.sender;
	}

	function setOwner(address _owner) public {
		owner = _owner;
	}

	function f() public {
		address prevOwner = owner;
		uint z = s.f();
		assert(z == y);
		assert(prevOwner == owner);
	}

	function g() public view returns (uint) {
		return y;
	}
}
// ----
// Warning 5084: (92-102): Type conversion is not yet fully supported and might yield false positives.
// Warning 4661: (459-473): Assertion violation happens here
// Warning 4661: (477-503): Assertion violation happens here
