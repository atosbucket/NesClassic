contract C {
    function f() public returns (address) {
        return msg.sender;
    }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f() -> 0x1212121212121212121212121212120000000012
