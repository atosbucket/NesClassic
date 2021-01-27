pragma experimental SMTChecker;

contract C {
	function k(bytes memory b0, bytes memory b1) public pure {
		bytes32 k0 = keccak256(b0);
		bytes32 k1 = keccak256(b1);
		assert(k0 == k1);
	}
	function s(bytes memory b0, bytes memory b1) public pure {
		bytes32 s0 = sha256(b0);
		bytes32 s1 = sha256(b1);
		assert(s0 == s1);
	}
	function r(bytes memory b0, bytes memory b1) public pure {
		bytes32 r0 = ripemd160(b0);
		bytes32 r1 = ripemd160(b1);
		assert(r0 == r1);
	}
	function e(bytes32 h0, uint8 v0, bytes32 r0, bytes32 s0, bytes32 h1, uint8 v1, bytes32 r1, bytes32 s1) public pure {
		address a0 = ecrecover(h0, v0, r0, s0);
		address a1 = ecrecover(h1, v1, r1, s1);
		assert(a0 == a1);
	}
}
// ----
// Warning 1218: (168-184): CHC: Error trying to invoke SMT solver.
// Warning 6328: (168-184): CHC: Assertion violation might happen here.
// Warning 1218: (305-321): CHC: Error trying to invoke SMT solver.
// Warning 6328: (305-321): CHC: Assertion violation might happen here.
// Warning 1218: (448-464): CHC: Error trying to invoke SMT solver.
// Warning 6328: (448-464): CHC: Assertion violation might happen here.
// Warning 6328: (673-689): CHC: Assertion violation happens here.\nCounterexample:\n\nh0 = 7719\nv0 = 21\nr0 = 10450\ns0 = 8365\nh1 = 38\nv1 = 151\nr1 = 32285\ns1 = 2437\n\nTransaction trace:\nC.constructor()\nC.e(7719, 21, 10450, 8365, 38, 151, 32285, 2437)
// Warning 4661: (168-184): BMC: Assertion violation happens here.
// Warning 4661: (305-321): BMC: Assertion violation happens here.
// Warning 4661: (448-464): BMC: Assertion violation happens here.
