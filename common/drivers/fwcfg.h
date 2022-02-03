#ifndef __DRIVERS__FWCFG_H__
#define __DRIVERS__FWCFG_H__

#include <stdint.h>
#include <lib/blib.h>
#include <lib/libc.h>

bool fwcfg_open(struct file_handle *handle, const char *name);

#endif
