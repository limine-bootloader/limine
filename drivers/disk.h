#ifndef __DISK_H__
#define __DISK_H__

int read_sector(int drive, int lba, int count, unsigned char *buffer);
int write_sector(int drive, int lba, int count, unsigned char *buffer);

#endif
