pragma experimental SMTChecker;
contract C {
    function f(bool x, bool y) public pure {
        assert(x == y);
    }
}
// ----
// Warning 6328: (98-112): CHC: Assertion violation happens here.
