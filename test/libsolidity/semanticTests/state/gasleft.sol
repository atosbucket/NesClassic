contract C {
    function f() public returns (bool) {
        return gasleft() > 0;
    }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f() -> true
// f() -> true
// f() -> true
