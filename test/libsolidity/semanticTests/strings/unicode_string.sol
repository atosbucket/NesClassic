contract C {
    function f() public pure returns (string memory) {
        return "😃, 😭, and 😈";
    }
}
// ====
// compileViaYul: also
// ----
// f() -> 0x20, 0x14, "\xf0\x9f\x98\x83, \xf0\x9f\x98\xad, and \xf0\x9f\x98\x88"
