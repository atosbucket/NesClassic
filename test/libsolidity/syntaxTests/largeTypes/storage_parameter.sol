contract C {
    struct S { uint256[2**255] x; }
    function f(S storage) internal {}
}
// ----
// Warning 7325: (64-65): Type struct C.S covers a large part of storage and thus makes collisions likely. Either use mappings or dynamic arrays and allow their size to be increased only in small quantities per transaction.
