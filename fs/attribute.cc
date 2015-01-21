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

#include "attribute.h"

using namespace std;

int ndnfs_getattr(const char *path, struct stat *stbuf)
{
#ifdef NDNFS_DEBUG
    cout << "ndnfs_getattr: path=" << path << endl;
#endif

    char fullPath[PATH_MAX];
	abs_path(fullPath, path);
    
    int ret = lstat(fullPath, stbuf);
    
	if (ret == -1) {
	  cerr << "ndnfs_getattr: get_attr failed. Full path " << fullPath << ". Errno " << errno << endl;
	  return -errno;
	}
	return ret;
}


int ndnfs_chmod(const char *path, mode_t mode)
{
#ifdef NDNFS_DEBUG
    cout << "ndnfs_chmod: path=" << path << ", change mode to " << std::oct << mode << endl;
#endif
    
    char fullPath[PATH_MAX];
	abs_path(fullPath, path);
    
    int res = chmod(fullPath, mode);
    if (res == -1) {
      cerr << "ndnfs_chmod: chmod failed. Errno: " << -errno << endl;
      return -errno;
    }
    
    return 0;
}


// Dummy function to stop commands such as 'cp' from complaining

#ifdef NDNFS_OSXFUSE
int ndnfs_setxattr(const char *path, const char *name, const char *value, size_t size, int flags, uint32_t position)
#elif NDNFS_FUSE
int ndnfs_setxattr(const char *path, const char *name, const char *value, size_t size, int flags)
#endif
{
    return 0;
}
