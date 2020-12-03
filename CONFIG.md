# Limine configuration file

## Location of the config file

Limine scans for a config file on *the boot drive*. Every partition on the boot drive
is scanned sequentially (first partition first, last partition last) for the presence
of either a `/limine.cfg` or a `/boot/limine.cfg` file, in that order.

Once the file is located, Limine will use it as its config file. Other possible
candidates in subsequent partitions or directories are ignored.

It is thus imperative that the intended config file is placed in a location that will
not be shadowed by another potentially candidate config file.

## Structure of the config file

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
* `TIMEOUT` - Specifies the timeout in seconds before the first *entry* is automatically booted. If set to `no`, disable automatic boot. If set to `0`, boots default entry instantly (see `DEFAULT_ENTRY` key).
* `DEFAULT_ENTRY` - 1-based entry index of the entry which will be automatically selected at startup. If unspecified, it is `1`.
* `GRAPHICS` - If set to `yes`, do use graphical VESA framebuffer for the boot menu, else use text mode.
* `MENU_RESOLUTION` - Specify screen resolution to be used by the Limine menu in the form `<width>x<height>`. This will *only* affect the menu, not any booted OS. If not specified, Limine will pick a resolution automatically. If the resolution is not available, Limine will pick another one automatically. Ignored if `GRAPHICS` is not `yes`.
* `THEME_COLOURS` - Specifies the colour palette used by the terminal (AARRGGBB). It is a `;` separated array of 8 colours: black, red, green, brown, blue, magenta, cyan, and gray, respectively. Ignored if `GRAPHICS` is not `yes`.
* `THEME_COLORS` - Alias of `THEME_COLOURS`.
* `THEME_MARGIN` - Set the amount of margin around the terminal. Ignored if `GRAPHICS` is not `yes`.
* `THEME_MARGIN_GRADIENT` - Set the thickness in pixel for the gradient around the terminal. Ignored if `GRAPHICS` is not `yes`.
* `BACKGROUND_PATH` - URI where to find the background .BMP file. Ignored if `GRAPHICS` is not `yes`.
* `STAGE2_MAP` - URI where to find the stage2 symbol map file for stacktraces and symbol name resolution at runtime.

*Locally assignable (non protocol specific)* keys are:
* `PROTOCOL` - The boot protocol that will be used to boot the kernel. Valid protocols are: `linux`, `stivale`, `stivale2`, `chainload`.
* `CMDLINE` - The command line string to be passed to the kernel. Can be omitted.
* `KERNEL_CMDLINE` - Alias of `CMDLINE`.

*Locally assignable (protocol specific)* keys are:
* Linux protocol:
  * `KERNEL_PATH` - The URI path of the kernel.
  * `MODULE_PATH` - The URI path to a module (such as initramfs).

  Note that one can define this last variable multiple times to specify multiple
  modules.
* stivale and stivale2 protocols:
  * `KERNEL_PATH` - The URI path of the kernel.
  * `MODULE_PATH` - The URI path to a module.
  * `MODULE_STRING` - A string to be passed to a module.

  Note that one can define these 2 last variable multiple times to specify multiple
  modules.
  The entries will be matched in order. E.g.: the 1st module path entry will be matched
  to the 1st module string entry that appear, and so on.
  * `RESOLUTION` - The resolution to be used should the kernel request a graphical framebuffer. This setting takes the form of `<width>x<height>x<bpp>` and *overrides* any resolution requested by the kernel, or automatic resolution requests. If the resolution is not available, Limine will pick another one automatically.
* Chainload protocol:
  * `DRIVE` - The 1-based BIOS drive to chainload.
  * `PARTITION` - The 1-based BIOS partition to chainload, if omitted, chainload drive.

## URIs

A URI is a path that Limine uses to locate resources in the whole system. It is
comprised of a *resource*, a *root*, and a *path*. It takes the form of:
```
resource://root/path
```

The format for `root` changes depending on the resource used.

A resource can be one of the following:
* `bios` - The `root` takes the form of `drive:partition`; for example: `bios://3:1/...` would use BIOS drive 3, partition 1. Partitions and BIOS drives are both 1-based. Omitting the drive is possible; for example: `bios://:2/...`. Omitting the drive makes Limine use the boot drive.
* `guid` - The `root` takes the form of a GUID/UUID, such as `guid://736b5698-5ae1-4dff-be2c-ef8f44a61c52/...`. It is a filesystem GUID and not a partition GUID.
* `tftp` - The `root` is the ip address of the tftp server to load the file from, if the root is left empty (`tftp:///file.elf`) the file will be loaded from the server limine booted from.
