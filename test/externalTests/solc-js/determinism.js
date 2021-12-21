const tape = require('tape');
const fs = require('fs');
const solc = require('../index.js');

tape('Deterministic Compilation', function (t) {
  t.test('DAO', function (st) {
    const input = {};
    let prevBytecode = null;
    const testdir = 'test/DAO/';
    const files = ['DAO.sol', 'Token.sol', 'TokenCreation.sol', 'ManagedAccount.sol'];
    let i;
    for (i in files) {
      const file = files[i];
      input[file] = { content: fs.readFileSync(testdir + file, 'utf8') };
    }
    for (i = 0; i < 10; i++) {
      const output = JSON.parse(solc.compile(JSON.stringify({
        language: 'Solidity',
        settings: {
          optimizer: {
            enabled: true
          },
          outputSelection: {
            '*': {
              '*': ['evm.bytecode']
            }
          }
        },
        sources: input
      })));
      st.ok(output);
      st.ok(output.contracts);
      st.ok(output.contracts['DAO.sol']);
      st.ok(output.contracts['DAO.sol'].DAO);
      st.ok(output.contracts['DAO.sol'].DAO.evm.bytecode.object);
      const bytecode = output.contracts['DAO.sol'].DAO.evm.bytecode.object;
      st.ok(bytecode.length > 0);
      if (prevBytecode !== null) {
        st.equal(prevBytecode, bytecode);
      }
      prevBytecode = bytecode;
    }
    st.end();
  });
});
