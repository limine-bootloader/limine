# Limine configuration file

The Limine configuration file is comprised of *assignments* and *entries*.

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

Some keys take *URIs* as values; these are described in the next section.

*Globally assignable* keys are:
* `TIMEOUT` - Specifies the timeout in seconds before the first *entry* is automatically booted.
* `DEFAULT_ENTRY` - 0-based entry index of the entry which will be automatically selected at startup. If unspecified, it is `0`.
* `GRAPHICS` - If set to `yes`, do use graphical VESA framebuffer for the boot menu, else use text mode.
* `THEME_BLACK`, `THEME_RED`, `THEME_GREEN`, `THEME_BROWN`, `THEME_BLUE`, `THEME_MAGENTA`, `THEME_CYAN`, `THEME_GREY` - Specifies the colors used by the terminal (RRGGBB). Ignored if `GRAPHICS` is not `yes`.
* `THEME_MARGIN` - Set the amount of margin around the terminal. Ignored if `GRAPHICS` is not `yes`.
* `THEME_MARGIN_GRADIENT` - Set the thickness in pixel for the gradient around the terminal. Ignored if `GRAPHICS` is not `yes`.
* `BACKGROUND_DRIVE` - Drive where to find the background .BMP file. Assume boot drive if unspecified. Ignored if `GRAPHICS` is not `yes`.
* `BACKGROUND_PATH` - Path where to find the background .BMP file. Ignored if `GRAPHICS` is not `yes`.

*Locally assignable (non protocol specific)* keys are:
* `PROTOCOL` - The boot protocol that will be used to boot the kernel. Valid protocols are: `linux`, `stivale`, `stivale2`, `chainload`.
* `CMDLINE` - The command line string to be passed to the kernel. Can be omitted.
* `KERNEL_CMDLINE` - Alias of `CMDLINE`.

*Locally assignable (protocol specific)* keys are:
* Linux protocol:
  * `KERNEL_PATH` - The URI path of the kernel.
  * `INITRD_PATH` - The URI path to the initial ramdisk.
* stivale and stivale2 protocols:
  * `KERNEL_PATH` - The URI path of the kernel.
  * `MODULE_PATH` - The URI path to a module.
  * `MODULE_STRING` - A string to be passed to a module.
* Chainload protocol:
  * `DRIVE` - The URI to a drive to chainload.

  Note that one can define these 3 variable multiple times to specify multiple modules.
  The entries will be matched in order. E.g.: the 1st partition entry will be matched
  to the 1st path and the 1st string entry that appear, and so on.

## URIs

A URI is a path that Limine uses to locate resources in the whole system. It is
comprised of a *resource*, a *root*, and a *path*. It takes the form of:
```
resource://root/path
```

The format for `root` changes depending on the resource used.

A resource can be one of the following:
* `bios` - The `root` takes the form of `drive:partition`; for example: `bios://3:1/...` would use BIOS drive 3, partition 1. Partitions and BIOS drives are both 1-based. Omitting the drive is possible; for example: `bios://:2/...`. Omitting the drive makes Limine use the boot drive.
* `guid` - The `root` takes the form of a GUID/UUID, such as `736b5698-5ae1-4dff-be2c-ef8f44a61c52`. It is a filesystem GUID and not a partition GUID.
