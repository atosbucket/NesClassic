{
    let a:u256
    { }
    function f() -> x:bool {
        let b:u256 := 4:u256
        {}
        for {} f() {} {}
    }
}
// ====
// step: mainFunction
// dialect: yul
// ----
// {
//     function main()
//     {
//         let a:u256
//         { }
//     }
//     function f() -> x:bool
//     {
//         let b:u256 := 4:u256
//         { }
//         for { } f() { }
//         { }
//     }
// }
