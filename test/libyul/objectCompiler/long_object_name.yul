object "t" {
	code {
		sstore(0, datasize("name_that_is_longer_than_32_bytes_name_that_is_longer_than_32_bytes_name_that_is_longer_than_32_bytes"))
	}
	object "name_that_is_longer_than_32_bytes_name_that_is_longer_than_32_bytes_name_that_is_longer_than_32_bytes" {
		code {}
	}
}
// ====
// optimizationPreset: full
// ----
// Assembly:
//     /* "source":33:146   */
//   dataSize(sub_0)
//     /* "source":30:31   */
//   0x00
//     /* "source":23:147   */
//   sstore
// stop
//
// sub_0: assembly {
// }
// Bytecode: 6000600055fe
// Opcodes: PUSH1 0x0 PUSH1 0x0 SSTORE INVALID
// SourceMappings: 33:113:0:-:0;30:1;23:124
