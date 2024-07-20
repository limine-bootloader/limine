# Limine's Design Philosophy

### Why not support filesystem X or feature Y? (eg: LUKS, LVM)

The idea with Limine is to remove the responsibility of parsing filesystems and formats, aside from the bare minimum necessities (eg: FAT*, ISO9660),
from the bootloader itself.
It is a needless duplication of efforts to have bootloaders support all possible filesystems and formats, and it leads to massive, bloated
bootloaders as a result (eg: GRUB2).
What is needed is to simply make sure the bootloader is capable of reading its own files, configuration, and be able to load kernel/module files
from disk. The kernel should be responsible for parsing everything else as it sees fit.

### What about LUKS? What about security? Encrypt the kernel!

Simply put, this is unnecessary. Putting the kernel/modules in a readable FAT32 partition and letting Limine know about their BLAKE2B checksums
in the config file provides as much security as encrypting the kernel does.

### What? But what if someone modifies the config file! Ha! You clearly have not thought about that!

We have. While this is a pointless effort on legacy x86 BIOS, it is a reasonable expectation on UEFI systems with Secure Boot. Limine provides a
way to modify its own EFI executable to bake in the BLAKE2B checksum of the config file itself. The EFI executable can then get signed with
a key added to the firmware's keychain. This prevents modifications to the config file (and in turn the checksums contained there)
from going unnoticed.

### What about ext2/3/4? Why is that supported then?

This is explicitly against the philosophy, but it is a pragmatic compromise since a lot of Linux distros and setups expect it to "work that way". It is necessary to keep in mind, additionally, that this is experimental and not explicitly supported, due to lack of maintenance. Maintainers wanted!

### But I don't want to have a separate FAT boot partition! I don't want it!!!

Well tough luck. It is `$year_following_2012` now and most PCs are equipped with UEFI and simply won't boot without a FAT EFI system partition
anyways. It is not unreasonable to share the EFI system partition with the OS's /boot and store kernels and initramfses there.
