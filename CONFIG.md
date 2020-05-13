# qloader2 configuration file

The qloader2 configuration file is comprised of *assignments* and *entries*.

*Entries* describe boot *entries* which the user can select in the *boot menu*.

An *entry* is simply a line starting with `:` followed by a newline-terminated
string.
Any *locally assignable* key that comes after it, and before another *entry*, or
the end of the file, will be tied to the *entry*.

*Assignments* are simple `KEY=VALUE` style assignments.
`VALUE` can have spaces and `=` symbols, without requiring quotations. New lines
are delimiters.

Some *assignments* are part of an entry (*local*), some other assignments are *global*.
*Global assignments* can appear anywhere in the file and are not part of an entry,
although usually one would put them at the beginning of the config.
Some *local assignments* are shared between entries using any *protocol*, while other
*local assignments* are specific to a given *protocol*.

*Globally assignable* keys are:
* `TIMEOUT` - Specifies the timeout in seconds before the first *entry* is automatically booted.

*Locally assignable (non protocol specific)* keys are:
* `PROTOCOL` - The boot protocol that will be used to boot the kernel. Valid protocols are: `linux`, `stivale`, `chainload`.
* `KERNEL_PROTO` - Alias of `PROTOCOL`.
* `CMDLINE` - The command line string to be passed to the kernel. Can be omitted.
* `KERNEL_CMDLINE` - Alias of `CMDLINE`.

*Locally assignable (protocol specific)* keys are:
* Linux protocol:
  * `KERNEL_DRIVE` - The BIOS drive (in decimal) where the kernel resides (if unspecified, boot drive is assumed).
  * `KERNEL_PARTITION` - The index (in decimal) of the partition containing the kernel.
  * `KERNEL_PATH` - The path of the kernel in said partition, forward slashes to delimit directories.
  * `INITRD_PARTITION` - Partition index of the initial ramdisk.
  * `INITRD_PATH` - The path to the initial ramdisk.
* stivale protocol:
  * `KERNEL_DRIVE` - The BIOS drive (in decimal) where the kernel resides (if unspecified, boot drive is assumed).
  * `KERNEL_PARTITION` - The index (in decimal) of the partition containing the kernel.
  * `KERNEL_PATH` - The path of the kernel in said partition, forward slashes to delimit directories.
  * `MODULE_PARTITION` - Partition index of a module.
  * `MODULE_PATH` - The path to a module.
  * `MODULE_STRING` - A string to be passed to a module.
* Chainload protocol:
  * `DRIVE` - The BIOS drive (in decimal) to chainload.
  * `PARTITION` - The partition index (in decimal) to chainload (if omitted, chainload the drive's bootsector).

  Note that one can define these 3 variable multiple times to specify multiple modules.
  The entries will be matched in order. E.g.: the 1st partition entry will be matched
  to the 1st path and the 1st string entry that appear, and so on.
