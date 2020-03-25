test.elf: test.asm linker.ld
	nasm test.asm -felf64 -o test.o
	ld test.o -no-pie -nostdlib -T ./linker.ld -o test.elf
