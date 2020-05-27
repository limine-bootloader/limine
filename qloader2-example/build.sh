# Assemble prekernel.asm.
nasm -f elf64 src/prekernel.asm -o bin/prekernel.o

# Compile kernel.c
x86_64-elf-gcc -g -fno-pic         \
    -mno-sse                       \
    -mno-sse2                      \
    -mno-mmx                       \
    -mno-80387                     \
    -mno-red-zone                  \
    -mcmodel=kernel                \
    -ffreestanding                 \
    -fno-stack-protector           \
    -O2                            \
    -fno-omit-frame-pointer        \
    -I src                         \
    -c src/kernel.c                \
    -o bin/kernel.o

# Link.
x86_64-elf-ld bin/prekernel.o bin/kernel.o -no-pie -nostdlib -T src/linker.ld -o bin/kernel.elf

# Create a disk image.
# We first create an empty disk image and then we format it
# to be able to use echfs (a filesystem) to eventally put 
# inside the kernel and configuration files.
dd if=/dev/zero bs=1M count=0 seek=64 of=disk/os.img
parted -s disk/os.img mklabel msdos
parted -s disk/os.img mkpart primary 1 100%
echfs-utils -m -p0 disk/os.img quick-format 32768
echfs-utils -m -p0 disk/os.img import src/qloader2.cfg qloader2.cfg
echfs-utils -m -p0 disk/os.img import bin/kernel.elf kernel.elf

# Now, install qloader2 on it.
./qloader2-install ./qloader2.bin disk/os.img

# And finally run it.
qemu-system-x86_64 -hda disk/os.img
