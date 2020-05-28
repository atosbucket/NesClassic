contract C {
  function f() {
    assembly {
      c := add(add(1, 2), c)
    }
  }
  int constant c = 0 + 1;
}
// ----
// SyntaxError: (15-83): No visibility specified. Did you intend to add "public"?
// TypeError: (71-72): Constant variables with non-literal values cannot be forward referenced from inline assembly.
// TypeError: (51-52): Constant variables cannot be assigned to.
