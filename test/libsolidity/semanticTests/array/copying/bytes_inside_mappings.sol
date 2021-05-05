contract c {
    function set(uint key) public returns (bool) { data[key] = msg.data; return true; }
    function copy(uint from, uint to) public returns (bool) { data[to] = data[from]; return true; }
    mapping(uint => bytes) data;
}
// ====
// compileViaYul: also
// ----
// set(uint256): 1, 2 -> true
// gas irOptimized: 103224
// gas legacy: 103491
// gas legacyOptimized: 103136
// set(uint256): 2, 2, 3, 4, 5 -> true
// gas irOptimized: 163911
// gas legacy: 164121
// gas legacyOptimized: 163766
// storageEmpty -> 0
// copy(uint256,uint256): 1, 2 -> true
// storageEmpty -> 0
// copy(uint256,uint256): 99, 1 -> true
// storageEmpty -> 0
// copy(uint256,uint256): 99, 2 -> true
// storageEmpty -> 1
