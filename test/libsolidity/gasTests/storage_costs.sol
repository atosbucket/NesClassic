contract C {
    uint x;
    function setX(uint y) public {
        x = y;
    }
    function resetX() public {
        x = 0;
    }
    function readX() public view returns(uint) {
        return x;
    }
}
// ====
// optimize: true
// optimize-yul: true
// ----
// creation:
//   codeDepositCost: 27000
//   executionCost: 81
//   totalCost: 27081
// external:
//   readX(): 2290
//   resetX(): 5116
//   setX(uint256): 22312
