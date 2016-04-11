/*
 * Copyright (c) 2013 University of California, Los Angeles
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Wentao Shang <wentao@cs.ucla.edu>
 *         Qiuhan Ding <dingqiuhan@gmail.com>
 */

#ifndef NDNFS_ATTRIBUTE_H
#define NDNFS_ATTRIBUTE_H

#include "ndnfs.h"
#include "version.h"
#include <sys/types.h>
#include <sys/xattr.h>

int ndnfs_getattr(const char *path, struct stat *stbuf);

int ndnfs_chmod(const char *path, mode_t mode);

#ifdef NDNFS_OSXFUSE
int ndnfs_setxattr(const char *path, const char *name, const char *value, size_t size, int flags, uint32_t position);
#elif NDNFS_FUSE
#ifdef HAVE_SETXATTR 
int ndnfs_setxattr(const char *path, const char *name, const char *value, size_t size, int flags);

int ndnfs_getxattr(const char *path, const char *name, char *value, size_t size);

int ndnfs_listxattr(const char* path, char* list, size_t size);

int ndnfs_removexattr(const char *path, const char *name);
#endif
#endif
#endif
