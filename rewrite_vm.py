import re

with open("source/dsp/ExpressionVM.h", "r") as f:
    content = f.read()

# I will just write a python script to replace the enum VarID and hardcoded arrays with dynamic maps.

