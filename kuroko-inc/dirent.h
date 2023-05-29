#pragma once
#include <stdint.h>

typedef struct dirent {
	int d_ino;
	char d_name[256];
} dirent;

typedef void * DIR;

extern DIR * opendir(const char * name);
extern int closedir(DIR * dirp);
extern struct dirent * readdir(DIR *dirp);
