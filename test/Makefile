test.elf: test.asm linker.ld
	nasm test.asm -felf32 -o test.o
	../toolchain/bin/i386-elf-ld test.o -nostdlib -T ./linker.ld -o test.elf
