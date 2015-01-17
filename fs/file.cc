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
  sqlite3_prepare_v2 (db, "SELECT current_version FROM file_system WHERE path = ?;", -1, &stmt, 0);
  sqlite3_bind_text (stmt, 1, path, -1, SQLITE_STATIC);
  int res = sqlite3_step (stmt);
  if (res != SQLITE_ROW) {
	sqlite3_finalize (stmt);
	return -ENOENT;
  }
    
  int curr_ver = sqlite3_column_int (stmt, 0);
  sqlite3_finalize (stmt);
  
  int temp_ver = time(0);
      
  switch (fi->flags & O_ACCMODE) {
    case O_RDONLY:
      // Should we also update version in this case (since the atime has changed)?
      break;
    case O_WRONLY:
    case O_RDWR:
      
      // Copy old data from current version to the temp version
      if (duplicate_version (path, curr_ver, temp_ver) < 0)
        return -EACCES;

      break;
    default:
      break;
  }
  
  return 0;
}

/**
 * Create function is replaced with mknod
 */
int ndnfs_mknod (const char *path, mode_t mode, dev_t dev)
{
#ifdef NDNFS_DEBUG
  cout << "ndnfs_mknod: path=" << path << ", mode=0" << std::oct << mode << endl;
#endif

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

  // TODO: We cannot create file without creating necessary folders in advance
  
  // Infer the mime_type of the file based on extension
  char mime_type[100] = "";
  mime_infer(mime_type, path);
  
  // Generate first version entry for the new file
  int ver = time(0);
  
  sqlite3_prepare_v2(db, "INSERT INTO file_versions (path, version) VALUES (?, ?);", -1, &stmt, 0);
  sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
  sqlite3_bind_int(stmt, 2, ver);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  // Add the file entry to database
  sqlite3_prepare_v2(db, 
					 "INSERT INTO file_system \
					  (path, current_version, mime_type, ready_signed) \
					  VALUES (?, ?, ?, ?);", 
					 -1, &stmt, 0);
  sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
  sqlite3_bind_int(stmt, 2, ver);  // current version
  sqlite3_bind_text(stmt, 3, mime_type, -1, SQLITE_STATIC); // mime_type based on ext
  
  enum SignatureState signatureState = NOT_READY;
  sqlite3_bind_int(stmt, 4, signatureState);
  
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  
  // Create the actual file
  char fullPath[PATH_MAX];
  abs_path(fullPath, path);
  
  int ret = 0;
  
  // For OS other than Linux, calling open directly does not seem to be enough;
  // On OS X, open is called instead.
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
  
  // First check if the file entry exists in the database, 
  // this now presumes we don't want to do anything with older versions of the file
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2(db, "SELECT type, current_version FROM file_system WHERE path = ?;", -1, &stmt, 0);
  sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
  int res = sqlite3_step(stmt);
  if (res != SQLITE_ROW) {
	sqlite3_finalize(stmt);
	return -ENOENT;
  }

  sqlite3_finalize(stmt);
  
  // Then write read from the actual file
  char fullPath[PATH_MAX];
  abs_path(fullPath, path);
  
  int fd = open(fullPath, O_RDONLY);
  
  if (fd == -1) {
    cerr << "ndnfs_read: open error. Errno: " << errno << endl;
    return -errno;
  }
  
  int read_len = pread(fd, buf, size, offset);
  
  if (read_len < 0) {
    cerr << "ndnfs_read: read error. Errno: " << errno << endl;
    return -errno;
  }
  
  close(fd);
  return read_len;
}


int ndnfs_write (const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
#ifdef NDNFS_DEBUG
  cout << "ndnfs_write: path=" << path << std::dec << ", size=" << size << ", offset=" << offset << endl;
#endif
  
  // First check if the entry exists in the database
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2 (db, "SELECT current_version FROM file_system WHERE path = ?;", -1, &stmt, 0);
  sqlite3_bind_text (stmt, 1, path, -1, SQLITE_STATIC);
  int res = sqlite3_step (stmt);
  if (res != SQLITE_ROW) {
	sqlite3_finalize (stmt);
	return -ENOENT;
  }
  
  sqlite3_finalize (stmt);
  
  // Then write the actual file
  char fullPath[PATH_MAX];
  abs_path(fullPath, path);
  int fd = open(fullPath, O_RDWR);
  if (fd == -1) {
    cerr << "ndnfs_write: open error. Errno: " << errno << endl;
    return -errno;
  }

  int write_len = pwrite(fd, buf, size, offset);
  if (write_len < 0) {
    cerr << "ndnfs_write: write error. Errno: " << errno << endl;
    return -errno;
  }
  
  close(fd);
  
  return write_len;  // return the number of bytes written on success
}


int ndnfs_truncate (const char *path, off_t length)
{
  // First we check if the entry exists in database
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2 (db, "SELECT current_version FROM file_system WHERE path = ?;", -1, &stmt, 0);
  sqlite3_bind_text (stmt, 1, path, -1, SQLITE_STATIC);
  int res = sqlite3_step (stmt);
  if (res != SQLITE_ROW) {
	sqlite3_finalize (stmt);
	return -ENOENT;
  } 
  sqlite3_finalize (stmt);
    
  // Then we truncate the actual file
  char fullPath[PATH_MAX];
  abs_path(fullPath, path);
  
  int trunc_ret = truncate(fullPath, length);
  if (trunc_ret == -1) {
    cerr << "ndnfs_truncate: error. Full path " << fullPath << ". Errno " << errno << endl;
    return -errno;
  }
  
  return res;
}


int ndnfs_unlink(const char *path)
{
#ifdef NDNFS_DEBUG
  cout << "ndnfs_unlink: path=" << path << endl;
#endif

  // TODO: update remove_versions
  remove_file_entry(path);

  // Then, remove file entry
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2(db, "DELETE FROM file_system WHERE path = ?;", -1, &stmt, 0);
  sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
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

  // First we check if the file exists
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2 (db, "SELECT current_version FROM file_system WHERE path = ?;", -1, &stmt, 0);
  sqlite3_bind_text (stmt, 1, path, -1, SQLITE_STATIC);
  int res = sqlite3_step (stmt);
  if (res != SQLITE_ROW) {
	sqlite3_finalize (stmt);
	return -ENOENT;
  }
  
  sqlite3_finalize (stmt);

  if ((fi->flags & O_ACCMODE) != O_RDONLY) {
	
	sqlite3_prepare_v2 (db, "SELECT * FROM file_versions WHERE path = ?;", -1, &stmt, 0);
	sqlite3_bind_text (stmt, 1, path, -1, SQLITE_STATIC);
	int res = sqlite3_step (stmt);
    sqlite3_finalize (stmt);
	
	if (res != SQLITE_ROW) {
	  sqlite3_finalize (stmt);
	  return -1;
	} else {
	  int curr_version = time(0);
	  sqlite3_prepare_v2 (db, "UPDATE file_system SET current_version = ? WHERE path = ?;", -1, &stmt, 0);
	  sqlite3_bind_int (stmt, 1, curr_version);  // set current_version to the current timestamp
	  sqlite3_bind_text (stmt, 2, path, -1, SQLITE_STATIC);
	  sqlite3_step (stmt);
      sqlite3_finalize (stmt);
	  
	  sqlite3_prepare_v2 (db, "INSERT INTO file_versions (path, version) VALUES (?,?);", -1, &stmt, 0);
	  sqlite3_bind_text (stmt, 1, path, -1, SQLITE_STATIC);
	  sqlite3_bind_int (stmt, 2, curr_version);
	  int res = sqlite3_step (stmt);
	  sqlite3_finalize (stmt);
	  if (res != SQLITE_OK && res != SQLITE_DONE)
		return -1;
	  
      // TODO: since older version is removed anyway, it makes sense to rely on system 
      // function calls for multiple file accesses. Simplification of versioning method?
	  //if (curr_ver != -1)
	  //  remove_version (path, curr_ver);
	}
	
	// After releasing, start a new signing thread for the file; 
    // If a signing thread for the file in question has already started, kill that thread.
  
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

