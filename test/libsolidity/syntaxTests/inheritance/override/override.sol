contract A {
	int public testvar;
	function test() internal returns (uint256);
	function test2() internal returns (uint256);
}
contract X is A {
	int public override testvar;
	function test() internal override returns (uint256);
	function test2() internal override(A) returns (uint256);
}
// ----
