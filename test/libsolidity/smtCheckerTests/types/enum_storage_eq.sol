pragma experimental SMTChecker;

contract C
{
	enum D { Left, Right }
	D d;
	function f(D _d) public {
		d = _d;
		assert(d != _d);
	}
}
// ----
// Warning 6328: (115-130): CHC: Assertion violation happens here.
