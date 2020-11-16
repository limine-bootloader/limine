# Limine PXE support

The `limine-pxe.bin` binary is a valid pxe boot image, in order to set up limine with pxe you need to setup a dhcp server
with support for pxe booting, this can either be set up using a single dhcp server or your existing dhcp server and a proxy
dhcp server such as dnsmasq.

The limine configuration file is assumed to be on the server that it booted from, for further information on pxe specific
configuration consult `CONFIG.md`
