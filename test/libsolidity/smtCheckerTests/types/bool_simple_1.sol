pragma experimental SMTChecker;
contract C {
    function f(bool x) public pure {
        assert(x);
    }
}
// ----
// Warning 6328: (90-99): CHC: Assertion violation happens here.
