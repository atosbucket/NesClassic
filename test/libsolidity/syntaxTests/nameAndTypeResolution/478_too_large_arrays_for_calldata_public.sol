contract C {
    function f(uint[85678901234] memory a) pure public {
    }
}
// ----
// TypeError: (28-54): Type too large for memory.
