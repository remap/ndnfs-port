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
 *         Zhehao Wang <wangzhehao410305@gmail.com>
 */

#include "attribute.h"

using namespace std;

int ndnfs_getattr(const char *path, struct stat *stbuf)
{
  FILE_LOG(LOG_DEBUG) << "ndnfs_getattr: path=" << path << endl;

  char fullPath[PATH_MAX];
  abs_path(fullPath, path);
  
  int ret = lstat(fullPath, stbuf);
  
  if (ret == -1) {
	FILE_LOG(LOG_ERROR) << "ndnfs_getattr: get_attr failed. Full path " << fullPath << ". Errno " << errno << endl;
	return -errno;
  }
  return ret;
}


int ndnfs_chmod(const char *path, mode_t mode)
{
  FILE_LOG(LOG_DEBUG) << "ndnfs_chmod: path=" << path << ", change mode to " << std::oct << mode << endl;
  
  char fullPath[PATH_MAX];
  abs_path(fullPath, path);
  
  int res = chmod(fullPath, mode);
  if (res == -1) {
	FILE_LOG(LOG_ERROR) << "ndnfs_chmod: chmod failed. Errno: " << -errno << endl;
	return -errno;
  }
  return 0;
}

#ifdef HAVE_SETXATTR

int ndnfs_setxattr(const char *path, const char *name, const char *value, size_t size, int flags) {
    
    FILE_LOG(LOG_DEBUG) << "ndnfs_setxattr: path=" << path << ", set an attribute name : " << name << ", set an value : " << value <<endl;
    char fullPath[PATH_MAX];
    abs_path(fullPath, path);
    int ret = lsetxattr(fullPath, name, value, size, flags);

    if(ret == -1){
      FILE_LOG(LOG_ERROR) << "ndnfs_setxattr: setxattr failed. Errno: " << -errno << endl;
      return -errno;
    }
    return 0;
}


int ndnfs_getxattr(const char *path, const char *name, char *value, size_t size){

    
    char fullPath[PATH_MAX];
    abs_path(fullPath, path);
    size_t retValue = lgetxattr(fullPath, name, value, size);
   // FILE_LOG(LOG_DEBUG) << "ndnfs_getxattr: path=" << path << ", get an attribute name : " << name << ", get an value : " << value <<endl;
    if(retValue == -1){
         //FILE_LOG(LOG_ERROR) << "ndnfs_getxattr: getxattr failed. Errno: " << -errno << endl;
	 return -errno;
    }
    return retValue;

}


int ndnfs_listxattr(const char* path, char* list, size_t size){
    char fullPath[PATH_MAX];
    abs_path(fullPath, path);
    size_t retValue = llistxattr(fullPath, list, size);
    FILE_LOG(LOG_DEBUG) << "ndnfs_listxattr: path=" << path << ", get an list name : " << list << endl;
    if(retValue == -1){
         FILE_LOG(LOG_ERROR) << "ndnfs_listxattr: listxattr failed. Errno: " << -errno << endl;
	 return -errno;
    }
    return retValue;
}

int ndnfs_removexattr(const char *path, const char *name){
    char fullPath[PATH_MAX];
    abs_path(fullPath, path);
    
    int retValue = lremovexattr(fullPath, name);
    
    
    FILE_LOG(LOG_DEBUG) << "ndnfs_removexattr: path=" << path << endl;
    if(retValue == -1){
         FILE_LOG(LOG_ERROR) << "ndnfs_removexattr: removexattr failed. Errno: " << -errno << endl;
	 return -errno;
    }
    return retValue;
}
#endif


// Dummy function to stop commands such as 'cp' from complaining


