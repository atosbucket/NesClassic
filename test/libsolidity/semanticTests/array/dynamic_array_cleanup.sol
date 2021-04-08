contract c {
    uint[20] spacer;
    uint[] dynamic;
    function fill() public {
        for (uint i = 0; i < 21; ++i)
            dynamic.push(i + 1);
    }
    function halfClear() public {
        while (dynamic.length > 5)
            dynamic.pop();
    }
    function fullClear() public { delete dynamic; }
}
// ====
// compileViaYul: also
// ----
// storage: empty
// fill() ->
// gas irOptimized: 535914
// gas legacy: 504373
// gas legacyOptimized: 499648
// storage: nonempty
// halfClear() ->
// storage: nonempty
// fullClear() ->
// storage: empty
