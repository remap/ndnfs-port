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

#include "directory.h"

using namespace std;

int ndnfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
  FILE_LOG(LOG_DEBUG) << "ndnfs_readdir: path=" << path << endl;

  DIR *dp;
  struct dirent *de;

  (void) offset;
  (void) fi;
  
  char fullPath[PATH_MAX];
  abs_path(fullPath, path);

  dp = opendir(fullPath);
  
  if (dp == NULL)
    return -errno;

  while ((de = readdir(dp)) != NULL) {
    struct stat st;
    memset(&st, 0, sizeof(st));
    st.st_ino = de->d_ino;
    st.st_mode = de->d_type << 12;
    if (filler(buf, de->d_name, &st, 0))
      break;
  }

  closedir(dp);
  return 0;
}

int ndnfs_mkdir(const char *path, mode_t mode)
{
  FILE_LOG(LOG_DEBUG) << "ndnfs_mkdir: path=" << path << ", mode=0" << std::oct << mode << endl;
  
  // TODO: test mk-sub-dir directly
  /*  
  string dir_path, dir_name;
  split_last_component(path, dir_path, dir_name);
  */
  
  char fullPath[PATH_MAX];
  abs_path(fullPath, path);
  int ret = mkdir(fullPath, mode);

  if (ret == -1) {
    FILE_LOG(LOG_ERROR) << "ndnfs_mkdir: mkdir failed. Errno: " << errno << endl;
    return -errno;
  }
  
  return 0;
}

/*
 * For rmdir, we don't need to implement recursive remove,
 * because 'rm -r' will iterate all the sub-entries (dirs or
 * files) for us and remove them one-by-one.   ---SWT
 */
int ndnfs_rmdir(const char *path)
{
  FILE_LOG(LOG_DEBUG) << "ndnfs_rmdir: path=" << path << endl;

  if (strcmp(path, "/") == 0) {
    // Cannot remove root dir.
    return -EINVAL;
  }
  
  char fullPath[PATH_MAX];
  abs_path(fullPath, path);
  int ret = rmdir(fullPath);

  if (ret == -1) {
    FILE_LOG(LOG_ERROR) << "ndnfs_rmdir: rmdir failed. Errno: " << errno << endl;
    return -errno;
  }
  
  return 0;
}
