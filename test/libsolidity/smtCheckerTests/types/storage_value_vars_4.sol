pragma experimental SMTChecker;
contract C
{
    function f() public view {
        assert(c > 0);
    }
    uint c;
}
// ----
// Warning 6328: (84-97): CHC: Assertion violation happens here.
