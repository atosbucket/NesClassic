contract c {
    function set() public returns (bool) {
        data = msg.data;
        return true;
    }

    function getLength() public returns (uint256) {
        return data.length;
    }

    bytes data;
}
// ====
// compileViaYul: also
// ----
// getLength() -> 0
// set(): 1, 2 -> true
// gas irOptimized: 103213
// gas legacy: 103126
// gas legacyOptimized: 102966
// getLength() -> 68
