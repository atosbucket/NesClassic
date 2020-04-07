contract C {
    uint immutable x = 3;
    constructor() public {
        delete x;
    }
}
// ----
// TypeError: (81-82): Immutable variables cannot be read during contract creation time, which means they cannot be read in the constructor or any function or modifier called from it.
