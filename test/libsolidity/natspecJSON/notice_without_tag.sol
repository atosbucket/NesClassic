contract test {
    /// I do something awesome
    function mul(uint a) public returns (uint d) { return a * 7; }
}

// ----
// ----
// :test userdoc
// {
//     "kind": "user",
//     "methods":
//     {
//         "mul(uint256)":
//         {
//             "notice": "I do something awesome"
//         }
//     },
//     "version": 1
// }
