{
    for {
        let a := 20
    }
    lt(a, 40)
    {
        a := add(a, 2)
    }
    {
        a := a
        continue
    }
}

// ====
// step: deadCodeEliminator
// ----
// {
//     for {
//         let a := 20
//     }
//     lt(a, 40)
//     {
//         a := add(a, 2)
//     }
//     {
//         a := a
//         continue
//     }
// }
