# RISC-V Computer Architecture Labs

### Assembler
Takes in a RISC-V assembly code file and encodes each instruction into a hexadecimal value according to the RISC-V
specification. It's designed to perform a two-pass assembly process. In the first pass, it parses an input file to build a symbol table, 
distinguishes between the text and data segments, and stores instructions and data in their respective arrays. In the second pass, 
the simulator translates the parsed instructions into machine code by encoding various instruction types.

### Simulator
