{
  let x := calldataload(0)
  let y := calldataload(0)
  let z := sub(y, x)
  sstore(add(x, 0), z)
}
// ====
// optimizationPreset: full
// ----
// Assembly:
//     /* "source":26:27   */
//   0x00
//     /* "source":13:28   */
//   dup1
//   calldataload
//     /* "source":79:99   */
//   sstore
//     /* "source":0:101   */
//   stop
// Bytecode: 600080355500
// Opcodes: PUSH1 0x0 DUP1 CALLDATALOAD SSTORE STOP
// SourceMappings: 26:1:0:-:0;13:15;;79:20;0:101
