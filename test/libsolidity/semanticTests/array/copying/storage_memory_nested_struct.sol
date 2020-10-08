contract C {
    struct T { uint8 x; uint8 y; uint[] z; }
    T[3][] a;

    function f() public returns (uint8, uint8, uint, uint, uint, uint8, uint8, uint, uint, uint) {
        a.push();
        a.push();
        a[0][1].x = 11;
        a[0][1].y = 12;
        a[0][1].z.push(1);
        a[0][1].z.push(2);
        a[0][1].z.push(3);
        a[1][2].x = 21;
        a[1][2].y = 22;
        a[1][2].z.push(4);
        a[1][2].z.push(5);
        a[1][2].z.push(6);
        T[3][] memory m = a;
        return (
            m[0][1].x, m[0][1].y, m[0][1].z[0], m[0][1].z[1], m[0][1].z[2],
            m[1][2].x, m[1][2].y, m[1][2].z[0], m[1][2].z[1], m[1][2].z[2]
        );
    }
}
// ----
// f() -> 11, 0x0c, 1, 2, 3, 0x15, 22, 4, 5, 6
