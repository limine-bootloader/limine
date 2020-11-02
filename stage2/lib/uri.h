#ifndef __LIB__URI_H__
#define __LIB__URI_H__

#include <stdbool.h>
#include <fs/file.h>

bool uri_resolve(char *uri, char **resource, char **root, char **path);
bool uri_open(struct file_handle *fd, char *uri);

#endif
