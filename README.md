# RISC-V Computer Architecture Labs

### Assembler
Takes in a RISC-V assembly code file and encodes each instruction into a hexadecimal value according to the RISC-V
specification. It's designed to perform a two-pass assembly process. In the first pass, it parses the input file to build a symbol table, 
distinguish between the text and data segments, and stores instructions and data in their respective arrays. In the second pass, 
the simulator translates the parsed instructions into machine code by encoding the various instruction types.

### Simulator
Takes in a file containing hexadecimal values that represent encoded RISC-V instructions and their operands. It follows the 5 stage RISC-V pipeline, 
Instruction Fetch (IF), Instruction Decode (ID), Execute (EX), Memory Access (MEM), and Write Back (WB). In the later lab assignments, we added 
capabilities supporting data hazard detection, data forwarding, and control hazard handling. 
