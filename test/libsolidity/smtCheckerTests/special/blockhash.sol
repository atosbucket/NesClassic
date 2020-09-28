pragma experimental SMTChecker;

contract C
{
	function f(uint x) public payable {
		assert(blockhash(x) > 0);
		assert(blockhash(2) > 0);
		uint y = x;
		assert(blockhash(x) == blockhash(y));
	}
}
// ----
// Warning 6328: (85-109): CHC: Assertion violation happens here.
// Warning 6328: (113-137): CHC: Assertion violation happens here.
// Warning 6328: (155-191): CHC: Assertion violation happens here.
