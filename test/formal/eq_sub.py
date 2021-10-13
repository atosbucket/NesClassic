from opcodes import EQ, ISZERO, SUB
from rule import Rule
from z3 import BitVec

"""
Rule:
ISZERO(SUB(X, Y)) -> EQ(X, Y)
"""

rule = Rule()

n_bits = 256

# Input vars
X = BitVec('X', n_bits)
Y = BitVec('Y', n_bits)

# Non optimized result
nonopt = ISZERO(SUB(X, Y))

# Optimized result
opt = EQ(X, Y)

rule.check(nonopt, opt)
