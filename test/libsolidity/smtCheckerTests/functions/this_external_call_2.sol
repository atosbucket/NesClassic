pragma experimental SMTChecker;

contract C {
    uint a;
    function f(uint x) public {
        this.g(x);
        assert(a == x);
        assert(a != 42);
    }

    function g(uint x) public {
        a = x;
    }
}
// ----
// Warning 6328: (141-156): CHC: Assertion violation happens here.
