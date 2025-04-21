mu-riscv: mu-riscv.c
	gcc -Wall -g -O2 $^ -o $@

mu-assembler: mu-assembler.c
	gcc -Wall -g -O2 $^ -o $@

.PHONY: clean
clean:
	rm -rf *.o *~ mu-riscv
