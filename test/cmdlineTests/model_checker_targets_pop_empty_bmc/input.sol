pragma solidity >=0.0;
pragma experimental SMTChecker;
contract test {
	uint[] arr;
    function f(address payable a, uint x) public {
		require(x >= 0);
		--x;
		x + type(uint).max;
		2 / x;
		a.transfer(x);
		assert(x > 0);
		arr.pop();
    }
}