pragma experimental SMTChecker;

contract C
{
	mapping (address => uint) map;
	function f(address a, uint x) public view {
		assert(x != map[a]);
	}
}
// ----
// Warning 6328: (125-144): CHC: Assertion violation happens here.
