pragma experimental ABIEncoderV2;

contract C {
    struct S {
        uint256 a;
        uint256 b;
        bytes2 c;
    }

    function f(S calldata s) external pure returns (uint256, uint256, byte) {
        S memory m = s;
        return (m.a, m.b, m.c[1]);
    }
}

// ====
// compileViaYul: also
// ----
// f((uint256,uint256, bytes2)): 42, 23, "ab" -> 42, 23, "b"
