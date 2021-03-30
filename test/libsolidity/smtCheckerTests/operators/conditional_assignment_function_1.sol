pragma experimental SMTChecker;

contract C {
    function f(uint a) internal pure returns (bool b) {
        b = a > 5;
    }
    function g(uint a) public pure {
        uint c = f(a) ? 3 : 4;
        assert(c > 5);
    }
}
// ----
// Warning 6328: (203-216): CHC: Assertion violation happens here.\nCounterexample:\n\na = 6\nc = 3\n\nTransaction trace:\nC.constructor()\nC.g(6)\n    C.f(6) -- internal call
