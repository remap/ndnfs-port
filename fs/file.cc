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

#include "file.h"

#include "signature-states.h"

using namespace std;

int ndnfs_open (const char *path, struct fuse_file_info *fi)
{
  // The actual open operation
  char fullPath[PATH_MAX];
  abs_path(fullPath, path);
  
  int ret = 0;
  ret = open(fullPath, fi->flags);
  
  if (ret == -1) {
	cerr << "ndnfs_open: open failed. Full path: " << fullPath << ". Errno: " << -errno << endl;
	return -errno;
  }
  close(ret);
  
  // Ndnfs versioning operation
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2 (db, "SELECT type, current_version, temp_version FROM file_system WHERE path = ?;", -1, &stmt, 0);
  sqlite3_bind_text (stmt, 1, path, -1, SQLITE_STATIC);
  int res = sqlite3_step (stmt);
  if (res != SQLITE_ROW) {
	sqlite3_finalize (stmt);
	return -ENOENT;
  }
    
  int type = sqlite3_column_int (stmt, 0);
  int curr_ver = sqlite3_column_int (stmt, 1);
  int temp_ver = sqlite3_column_int (stmt, 2);
  sqlite3_finalize (stmt);

  if (type != ndnfs::file_type)
    return -EISDIR;
  
  switch (fi->flags & O_ACCMODE) {
    case O_RDONLY:
      // Should we also update version in this case (since the atime has changed)?
      break;
    case O_WRONLY:
    case O_RDWR:
      // Create temporary version for file editing
        
      // If there is already a temp version there, it means that someone is writing to the file now
      // We should reject this open request
      // This effectively implements some sort of file locking
      if (temp_ver != -1)
        return -EACCES;

      // Create a new version number for temp_ver based on system time
      temp_ver = time(0);
      
      // The possibility of (temp_ver == curr_ver) exists, especially in open call directly after mknod 
      if (temp_ver <= curr_ver) {
        temp_ver = curr_ver + 1;
      }
      
      // Copy old data from current version to the temp version
      // An version entry for temp_ver will be inserted in this function
      if (duplicate_version (path, curr_ver, temp_ver) < 0)
        return -EACCES;

      // Update file entry with temp version info
      sqlite3_prepare_v2 (db, "UPDATE file_system SET atime = ?, temp_version = ? WHERE path = ?;", -1, &stmt, 0);
      sqlite3_bind_int (stmt, 1, temp_ver);
      sqlite3_bind_int (stmt, 2, temp_ver);
      sqlite3_bind_text (stmt, 3, path, -1, SQLITE_STATIC);
      res = sqlite3_step (stmt);
      sqlite3_finalize (stmt);

      if (res != SQLITE_OK && res != SQLITE_DONE)
        return -EACCES;
      
      break;
    default:
      break;
  }
  
  return 0;
}

/**
 * Create function is replaced with mknod, which means instead of writing to the 
 * tmp_version field in the database (because create include open with RDWR), we
 * write to the current_version field.
 */
int ndnfs_mknod (const char *path, mode_t mode, dev_t dev)
{
#ifdef NDNFS_DEBUG
  cout << "ndnfs_mknod: path=" << path << ", mode=0" << std::oct << mode << endl;
#endif

  string dir_path, file_name;
  split_last_component(path, dir_path, file_name);
  
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2(db, "SELECT * FROM file_system WHERE path = ?;", -1, &stmt, 0);
  sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
  int res = sqlite3_step(stmt);
  if (res == SQLITE_ROW) {
	  // Cannot create file that has conflicting file name
	  sqlite3_finalize(stmt);
	  return -ENOENT;
  }
  
  sqlite3_finalize(stmt);

  //XXX: We don't check this for now.
  // // Cannot create file without creating necessary folders in advance
  
  // Infer the mime_type of the file based on extension
  char mime_type[100] = "";
  mime_infer(mime_type, path);
  
  // Generate first version for the new file
  int ver = time(0);
  
  // Create first version for the new file
  sqlite3_prepare_v2(db, "INSERT INTO file_versions (path, version, size, totalSegments) VALUES (?, ?, ?, ?);", -1, &stmt, 0);
  sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
  sqlite3_bind_int(stmt, 2, ver);
  sqlite3_bind_int(stmt, 3, 0);
  sqlite3_bind_int(stmt, 4, 0);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  // Add the file entry to database
  sqlite3_prepare_v2(db, 
					 "INSERT INTO file_system \
					  (path, parent, type, mode, atime, mtime, size, current_version, temp_version, mime_type) \
					  VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);", 
					 -1, &stmt, 0);
  sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, dir_path.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_int(stmt, 3, ndnfs::file_type);
  sqlite3_bind_int(stmt, 4, mode);
  sqlite3_bind_int(stmt, 5, ver);
  sqlite3_bind_int(stmt, 6, ver);
  sqlite3_bind_int(stmt, 7, 0);  // size
  sqlite3_bind_int(stmt, 8, ver);  // current version
  sqlite3_bind_int(stmt, 9, -1);  // temp version
  sqlite3_bind_text(stmt, 10, mime_type, -1, SQLITE_STATIC); // mime_type based on ext
  
  enum SignatureState signatureState = NOT_READY;
  sqlite3_bind_int(stmt, 11, signatureState);
  
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  // Update mtime for parent folder
  sqlite3_prepare_v2(db, "UPDATE file_system SET mtime = ? WHERE path = ?;", -1, &stmt, 0);
  sqlite3_bind_int(stmt, 1, ver);
  sqlite3_bind_text(stmt, 2, dir_path.c_str(), -1, SQLITE_STATIC);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  
  // Create the actual file
  char fullPath[PATH_MAX];
  abs_path(fullPath, path);
  
  int ret = 0;
  
  // For OS other than Linux, calling open directly does not seem to be enough;
  if (S_ISREG(mode)) {
    ret = open(fullPath, O_CREAT | O_EXCL | O_WRONLY, mode);
    if (ret >= 0) {
      ret = close(ret);
    }
  } else if (S_ISFIFO(mode)) {
	ret = mkfifo(fullPath, mode);
  } else {
	ret = mknod(fullPath, mode, dev);
  }
    
  if (ret == -1) {
    cerr << "ndnfs_mknod: mknod failed. Full path: " << fullPath << ". Errno " << errno << endl;
    return -errno;
  }
  
  return 0;
}


int ndnfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
#ifdef NDNFS_DEBUG
    cout << "ndnfs_read: path=" << path << ", offset=" << std::dec << offset << ", size=" << size << endl;
#endif

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "SELECT type, current_version FROM file_system WHERE path = ?;", -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
    int res = sqlite3_step(stmt);
    if (res != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return -ENOENT;
    }
    
    int type = sqlite3_column_int(stmt, 0);
    int curr_ver = sqlite3_column_int(stmt, 1);
    if (type != ndnfs::file_type) {
        sqlite3_finalize(stmt);
        return -EINVAL;
    }

    sqlite3_finalize(stmt);

    int size_read = read_version(path, curr_ver, buf, size, offset, fi);

    sqlite3_prepare_v2(db, "UPDATE file_system SET atime = ? WHERE path = ?;", -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, (int)time(0));
    sqlite3_bind_text(stmt, 2, path, -1, SQLITE_STATIC);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return size_read;
}


int ndnfs_write (const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
#ifdef NDNFS_DEBUG
  cout << "ndnfs_write: path=" << path << std::dec << ", size=" << size << ", offset=" << offset << endl;
#endif
  
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2 (db, "SELECT type, current_version, temp_version FROM file_system WHERE path = ?;", -1, &stmt, 0);
  sqlite3_bind_text (stmt, 1, path, -1, SQLITE_STATIC);
  int res = sqlite3_step (stmt);
  if (res != SQLITE_ROW) {
	sqlite3_finalize (stmt);
	return -ENOENT;
  }
  
  int type = sqlite3_column_int (stmt, 0);
  int curr_ver = sqlite3_column_int (stmt, 1);
  int temp_ver = sqlite3_column_int (stmt, 2);
  sqlite3_finalize (stmt);
  
  if (type != ndnfs::file_type)
    return -EINVAL;

  // Write data to a new version of the file
  int ver_size = write_version (path, temp_ver, buf, size, offset, fi);
  
  if (ver_size < 0)
    return -EINVAL;

  sqlite3_prepare_v2 (db, "UPDATE file_system SET size = ?, mtime = ? WHERE path = ?;", -1, &stmt, 0);
  sqlite3_bind_int (stmt, 1, ver_size);
  sqlite3_bind_int (stmt, 2, (int) time (0));
  sqlite3_bind_text (stmt, 3, path, -1, SQLITE_STATIC);
  res = sqlite3_step (stmt);
  sqlite3_finalize (stmt);

  if (res != SQLITE_OK && res != SQLITE_DONE)
    return -EACCES;
  
  return (int) size;  // return the number of bytes written on success
}


int ndnfs_truncate (const char *path, off_t length)
{
#ifdef NDNFS_DEBUG
  cout << "ndnfs_truncate: path=" << path << ", truncate to length " << std::dec << length << endl;
#endif

  sqlite3_stmt *stmt;
  sqlite3_prepare_v2 (db, "SELECT type, current_version, temp_version FROM file_system WHERE path = ?;", -1, &stmt, 0);
  sqlite3_bind_text (stmt, 1, path, -1, SQLITE_STATIC);
  int res = sqlite3_step (stmt);
  if (res != SQLITE_ROW) {
	sqlite3_finalize (stmt);
	return -ENOENT;
  }
    
  int type = sqlite3_column_int (stmt, 0);
  int curr_ver = sqlite3_column_int (stmt, 1);
  int temp_ver = sqlite3_column_int (stmt, 2);
  sqlite3_finalize (stmt);
    
  if (type != ndnfs::file_type)
    return -EINVAL;

  // TODO: fix truncate_version
  int ret = truncate_version (path, temp_ver, length);

  sqlite3_prepare_v2 (db, "UPDATE file_system SET size = ?, mtime = ? WHERE path = ?;", -1, &stmt, 0);
  sqlite3_bind_int (stmt, 1, (int)length);
  sqlite3_bind_int (stmt, 2, (int)time(0));
  sqlite3_bind_text (stmt, 3, path, -1, SQLITE_STATIC);
  sqlite3_step (stmt);
  sqlite3_finalize (stmt);
  
  return ret;
}


int ndnfs_unlink(const char *path)
{
#ifdef NDNFS_DEBUG
  cout << "ndnfs_unlink: path=" << path << endl;
#endif

  string dir_path, file_name;
  split_last_component(path, dir_path, file_name);

  // First, remove all the versions under the file entry (both current and temp)
  // TODO: fix remove_versions
  remove_versions(path);    

  // Then, remove file entry
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2(db, "DELETE FROM file_system WHERE type=1 AND path = ?;", -1, &stmt, 0);
  sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  
  // Finally, update parent directory mtime
  sqlite3_prepare_v2(db, "UPDATE file_system SET mtime = ? WHERE path = ?;", -1, &stmt, 0);
  sqlite3_bind_int(stmt, 1, (int)time(0));
  sqlite3_bind_text(stmt, 2, dir_path.c_str(), -1, SQLITE_STATIC);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  
  char fullPath[PATH_MAX];
  abs_path(fullPath, path);
  int ret = unlink(fullPath);
  
  if (ret == -1) {
    cerr << "ndnfs_unlink: unlink failed. Errno: " << errno << endl;
    return -errno;
  }
    
  return 0;
}

int ndnfs_release (const char *path, struct fuse_file_info *fi)
{
#ifdef NDNFS_DEBUG
  cout << "ndnfs_release: path=" << path << ", flag=0x" << std::hex << fi->flags << endl;
#endif

  sqlite3_stmt *stmt;
  sqlite3_prepare_v2 (db, "SELECT type, current_version, temp_version FROM file_system WHERE path = ?;", -1, &stmt, 0);
  sqlite3_bind_text (stmt, 1, path, -1, SQLITE_STATIC);
  int res = sqlite3_step (stmt);
  if (res != SQLITE_ROW) {
	sqlite3_finalize (stmt);
	return -ENOENT;
  }

  int type = sqlite3_column_int (stmt, 0);
  int curr_ver = sqlite3_column_int (stmt, 1);
  int temp_ver = sqlite3_column_int (stmt, 2);
  if (type != ndnfs::file_type) {
	sqlite3_finalize (stmt);
	return -EINVAL;
  }

  sqlite3_finalize (stmt);

  /*
    In FUSE design, if the file is created with create() call,
    the corresponding release() call always has fi->flags = 0.
    So we need to check the current version number to tell if
    the file is just created or opened in read only mode.
  */

  // Check open flags
  if (((fi->flags & O_ACCMODE) != O_RDONLY) || curr_ver == -1) {
    if (temp_ver == -1) {
      return -EINVAL;
    }
	
	sqlite3_prepare_v2 (db, "SELECT * FROM file_versions WHERE path = ? AND version = ?;", -1, &stmt, 0);
	sqlite3_bind_text (stmt, 1, path, -1, SQLITE_STATIC);
	sqlite3_bind_int (stmt, 2, temp_ver);
	int res = sqlite3_step (stmt);

	if (res != SQLITE_ROW) {
	  // Should not happen
	  sqlite3_finalize (stmt);
	  return -1;
	} else {
	  // Update version number and remove old version
	  int size = sqlite3_column_int (stmt, 2);

	  sqlite3_finalize (stmt);
	  sqlite3_prepare_v2 (db, "UPDATE file_system SET size = ?, current_version = ?, temp_version = ? WHERE path = ?;", -1, &stmt, 0);
	  sqlite3_bind_int (stmt, 1, size);
	  sqlite3_bind_int (stmt, 2, temp_ver);  // set current_version to the original temp_version
	  sqlite3_bind_int (stmt, 3, -1);  // set temp_version to -1
	  sqlite3_bind_text (stmt, 4, path, -1, SQLITE_STATIC);
	  sqlite3_step (stmt);

	  if (curr_ver != -1)
	    remove_version (path, curr_ver);
	}
    sqlite3_finalize (stmt);
  }
  
  return 0;
}

int ndnfs_statfs(const char *path, struct statvfs *si)
{
  //cout << "ndnfs_statfs: stat called." << endl;
  char fullPath[PATH_MAX];
  abs_path(fullPath, path);
  
  int ret = statvfs(fullPath, si);

  if (ret == -1) {
    cerr << "ndnfs_statfs: stat failed. Errno " << errno << endl;
	return -errno;
  }
  
  return 0;
}

int ndnfs_access(const char *path, int mask)
{
  //cout << "ndnfs_access: access called." << endl;
  char fullPath[PATH_MAX];
  abs_path(fullPath, path);
  
  int ret = access(fullPath, mask);

  if (ret == -1) {
    cerr << "ndnfs_access: access failed. Errno " << errno << endl;
	return -errno;
  }
  
  return 0;
}

