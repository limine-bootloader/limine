# qloader2 example usage

# What you need before building
**A GNU/Linux system. Most of these apps don't work on Windows.**  
**NASM**. You can get it from here: https://nasm.us/ or use your package manager to install it.  
**QEMU**. You will need this to emulate the disk image. You can get it from this website: https://www.qemu.org/ or use your package manager to install it.  
**x86_64-elf**. This will let you link and compile your sources. More specifically you need x86_64-elf-gcc and x86_64-elf-ld.  
**echfs-utils**. You will need to build this manually. Link: https://github.com/qword-os/echfs  

# Additional notes
Copy from the root directory of qloader2's repository qloader2-install and qloader2.bin. Then, paste it here.

# How to build
First, run `chmod u+x build.sh`. Then, run `./build.sh`. You might also want to clean up the bin and disk folder. If so, run `chmod u+x clean.sh` and then `./clean.sh`  

