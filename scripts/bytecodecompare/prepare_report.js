#!/usr/bin/env node
const process = require('process')
const fs = require('fs')

const compiler = require('./solc-js/wrapper.js')(require('./solc-js/soljson.js'))

for (const optimize of [false, true])
{
    for (const filename of process.argv.slice(2))
    {
        if (filename !== undefined)
        {
            const input = {
                language: 'Solidity',
                sources: {
                    [filename]: {content: fs.readFileSync(filename).toString()}
                },
                settings: {
                    optimizer: {enabled: optimize},
                    outputSelection: {'*': {'*': ['evm.bytecode.object', 'metadata']}},
                    "modelChecker": {"engine": "none"}
                }
            }

            const result = JSON.parse(compiler.compile(JSON.stringify(input)))

            if (
                !('contracts' in result) ||
                Object.keys(result['contracts']).length === 0 ||
                Object.keys(result['contracts']).every(file => Object.keys(result['contracts'][file]).length === 0)
            )
                // NOTE: do not exit here because this may be run on source which cannot be compiled
                console.log(filename + ': ERROR')
            else
                for (const contractFile in result['contracts'])
                    for (const contractName in result['contracts'][contractFile])
                    {
                        const contractResults = result['contracts'][contractFile][contractName]

                        let bytecode = 'NO BYTECODE'
                        if ('evm' in contractResults && 'bytecode' in contractResults['evm'] && 'object' in contractResults['evm']['bytecode'])
                            bytecode = contractResults.evm.bytecode.object

                        console.log(filename + ':' + contractName + ' ' + bytecode)
                        console.log(filename + ':' + contractName + ' ' + contractResults['metadata'])
                    }
        }
    }
}
