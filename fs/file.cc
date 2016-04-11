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

#include "file.h"
#include <ctime>
#include <cstdlib>
#include <iostream>
#include <chrono>
#include <cstring>
#include <sstream>
#include <string>


#include "signature-states.h"

using namespace std;

template<class To, class From>
To any_cast(From v)
{
    return static_cast<To>(static_cast<void*>(v));
}


int ndnfs_open (const char *path, struct fuse_file_info *fi)
{
  // The actual open operation
  char full_path[PATH_MAX];
  abs_path(full_path, path);
  
  int ret = 0;
  ret = open(full_path, fi->flags);
  
  if (ret == -1) {
    FILE_LOG(LOG_ERROR) << "ndnfs_open: open failed. Full path: " << full_path << ". Errno: " << -errno << endl;
    return -errno;
  }
  close(ret);
  
  // Ndnfs versioning operation
  /*sqlite3_stmt *stmt;
  sqlite3_prepare_v2 (db, "SELECT current_version FROM file_system WHERE path = ?;", -1, &stmt, 0);
  sqlite3_bind_text (stmt, 1, path, -1, SQLITE_STATIC);
  int res = sqlite3_step (stmt);
  if (res != SQLITE_ROW) {
    sqlite3_finalize (stmt);
    return -ENOENT;
  }*/
  
 
  char versionVal[PATH_MAX];
  //char* current_versionName = "user.current_version";
  int bufferLength = getxattr(full_path, current_versionName, versionVal, sizeof(versionVal));
  int curr_ver = atoi(versionVal);
  //version retrieval must be implemented.
  //sqlite3_finalize (stmt);
  
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
 * TODO:
 * In Linux(Ubuntu), current implementation reports "utimens: no such file" when executing touch; digging out why.
 * For the newly created file, getattr is called before mknod/open(O_CREAT); wonder how that works.
 */
int ndnfs_mknod (const char *path, mode_t mode, dev_t dev) // I am now only considering the insertion operation so that metadata JSON object is created over here only.
{
  FILE_LOG(LOG_DEBUG) << "ndnfs_mknod: path=" << path << ", mode=0" << std::oct << mode << endl;

/*
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2(db, "SELECT * FROM file_system WHERE path = ?;", -1, &stmt, 0);
  sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
  int res = sqlite3_step(stmt);
  if (res == SQLITE_ROW) {
      // Cannot create file that has conflicting file name
      sqlite3_finalize(stmt);
      return -ENOENT;
  }
  
  sqlite3_finalize(stmt);*/

  // TODO: We cannot create file without creating necessary folders in advance
  
  // Infer the mime_type of the file based on extension
  char mime_type[100] = "";
  mime_infer(mime_type, path);
  
  // Generate first version entry for the new file
  int ver = time(0);
  FILE_LOG(LOG_DEBUG) << "ndnfs_mknod: 1st version entry, Version integer is : " << ver << endl;
  /*
  sqlite3_prepare_v2(db, "INSERT INTO file_versions (path, version) VALUES (?, ?);", -1, &stmt, 0);
  sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
  sqlite3_bind_int(stmt, 2, ver);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  // Add the file entry to database
  sqlite3_prepare_v2(db, 
                     "INSERT INTO file_system \
                      (path, current_version, mime_type, ready_signed, type) \
                      VALUES (?, ?, ?, ?, ?);", 
                     -1, &stmt, 0);
  sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
  sqlite3_bind_int(stmt, 2, ver);  // current version
  sqlite3_bind_text(stmt, 3, mime_type, -1, SQLITE_STATIC); // mime_type based on ext
  */
  enum SignatureState signatureState = NOT_READY;
 // sqlite3_bind_int(stmt, 4, signatureState);
  
  enum FileType fileType = REGULAR;
 
  switch (S_IFMT & mode) 
  {
	case S_IFDIR: 
	  // expect this to call mkdir instead
	  break;
	case S_IFCHR:
	  fileType = CHARACTER_SPECIAL;
	  break;
	case S_IFREG: 
	  fileType = REGULAR;
	  break;
	case S_IFLNK: 
	  fileType = SYMBOLIC_LINK;
	  break;
	case S_IFSOCK: 
	  fileType = UNIX_SOCKET;
	  break;
	case S_IFIFO: 
	  fileType = FIFO_SPECIAL;
	  break;
    default:
      fileType = REGULAR;
      break;
  }
  /*sqlite3_bind_int(stmt, 5, fileType);
  
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);*/
  
  // Create the actual file
  char full_path[PATH_MAX];
  abs_path(full_path, path);
  
  int ret = 0;
  int fileTobeWritten = 0;
  std::string jsonMetadata = "";
  //convert metadata values to C string.
  char* pathValue = (char*)path;
  std::string versionString = std::to_string(ver);
  char* verValue = (char*)versionString.c_str();
  char* current_versionValue = (char*)versionString.c_str();
  char* mime_typeValue = &mime_type[0];
  std::string ready_signedString = std::to_string((int)signatureState);
  std::string fileTypeString = std::to_string((int)fileType);
  char* ready_signedValue = (char*)ready_signedString.c_str();
  char* fileTypeValue = (char*)fileTypeString.c_str();
  char* certificateValue = (char*)&ndnfs::certificateName;
  //char* metadataValues [] = {path, ver.c_str(), current_version.c_str(), mime_type, signatureState.c_str(), fileType.c_str()};//This array will be used later not now.
  //path is char* string. ver is int so that it must be converted to Cstring. Ready_signed is enum type, integer is converted to C String type.
  
 
  //char* metadataNames [] ={"path", "ver", "current_version","mime_type", "type"}; //it will be used later not now.
  if (S_ISREG(mode)) {
    ret = open(full_path, O_CREAT | O_EXCL | O_WRONLY, mode);
    if (ret >= 0) {
      ret = close(ret);
    }
  } else if (S_ISFIFO(mode)) {
    ret = mkfifo(full_path, mode);
  } else {
    ret = mknod(full_path, mode, dev);
  }
  //Meta data is encoded into JSON format.
  if (ret == -1) {
    FILE_LOG(LOG_ERROR) << "ndnfs_mknod: mknod failed. Full path: " << full_path << ". Errno " << errno << endl;
    return -errno;
  } else {
    FILE_LOG(LOG_DEBUG) << "ndnfs_file metadata : all metadata is encoded into JSON string format." << endl;
   
    
    //ret = open(full_path, O_RDWR, mode);

    int rc1 = setxattr(full_path, pathName, pathValue, strlen(pathValue), 0);
    if (rc1 != 0){
        FILE_LOG(LOG_ERROR) << "ndnfs_mknod: setxattr pathName failed :" << full_path << ". Errno " << errno << endl;
	FILE_LOG(LOG_ERROR) << "ndnfs_mknod: setxattr failed name :" << pathName << " pathValue: " << pathValue << endl;
        ret = close(ret);
        return -errno;
    } else {
        FILE_LOG(LOG_DEBUG) << "ndnfs_mknod: setxattr successful name :" << pathName << " pathValue: " << pathValue << endl;
    }
    int rc2 = setxattr(full_path, verName, verValue, strlen(verValue), 0);
    int rc3 = setxattr(full_path, current_versionName, current_versionValue, strlen(current_versionValue), 0);
    int rc4 = setxattr(full_path, mime_typeName, mime_typeValue, strlen(mime_typeValue), 0);
    int rc5 = setxattr(full_path, ready_signedName, ready_signedValue, strlen(ready_signedValue), 0);
    int rc6 = setxattr(full_path, fileTypeName, fileTypeValue, strlen(fileTypeValue), 0);
    int rc7 = setxattr(full_path, certificateNName, certificateValue, strlen(certificateValue), 0);
    FILE_LOG(LOG_DEBUG) << "ndnfs_mknod: ndnfs_file metadata, path value is written. Returned integer is : " << rc1 << rc2 << endl;
    FILE_LOG(LOG_DEBUG) << "ndnfs_mknod: ndnfs_file metadata, ver value is written. Ver is : " << verValue << endl;
    FILE_LOG(LOG_DEBUG) << "ndnfs_mknod: ndnfs_file metadata, current_version is written. Current_Ver is : " << current_versionValue << endl;
    FILE_LOG(LOG_DEBUG) << "ndnfs_mknod: ndnfs_file metadata, mime_type is written. Current_Ver is : " << mime_typeValue << endl;
    FILE_LOG(LOG_DEBUG) << "ndnfs_mknod: ndnfs_file metadata, ready_signed is written. readySigned is : " << ready_signedValue << endl;
    FILE_LOG(LOG_DEBUG) << "ndnfs_mknod: ndnfs_file metadata, fileType is written. Filetype is : " << fileTypeValue << endl;
    FILE_LOG(LOG_DEBUG) << "ndnfs_mknod: ndnfs_file metadata, certificate is written. certificate is : " << certificateValue << endl;
    //ret = close(ret);
    
  }
  
  return 0;
}




int ndnfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)//Currently no addtional code is added over here.
{
  FILE_LOG(LOG_DEBUG) << "ndnfs_read: path=" << path << ", offset=" << std::dec << offset << ", size=" << size << endl;
  
  // First check if the file entry exists in the database, 
  // this now presumes we don't want to do anything with older versions of the file
  // This SQLite statement has been replaced with In-File metadata.
  /*sqlite3_stmt *stmt; 
  sqlite3_prepare_v2(db, "SELECT current_version FROM file_system WHERE path = ?;", -1, &stmt, 0);
  sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
  int res = sqlite3_step(stmt);
  if (res != SQLITE_ROW) {
    sqlite3_finalize(stmt);
    return -ENOENT;
  }

  sqlite3_finalize(stmt);*/
  //Extended File Attribute Implementation
  char full_path[PATH_MAX];
  abs_path(full_path, path);
  char versionVal[PATH_MAX];
  //char* current_versionName = "user.current_version";
  int bufferLength = getxattr(full_path, current_versionName, versionVal, sizeof(versionVal));
  
  if (bufferLength < 0 ){
     FILE_LOG(LOG_ERROR) << "ndnfs_read: ext attribute open rror: Errno: " << errno << endl;
     return -ENOENT;
  } else {
      FILE_LOG(LOG_DEBUG) << "ndnfs_read: ext attribute open success: version value: " << versionVal << endl;
  }
  //I just check whether metadata entry exists live the comment above. Code below will continue to be implemented if neccessary 
  /*
  char current_version[bufferLength];
  getxattr(full_path, current_versionName, current_version, sizeof(current_version));*/
  
  
  // Then write read from the actual file
  
  
  int fd = open(full_path, O_RDONLY);
  
  if (fd == -1) {
    FILE_LOG(LOG_ERROR) << "ndnfs_read: open error. Errno: " << errno << endl;
    return -errno;
  }
  
  int read_len = pread(fd, buf, size, offset);
  
  if (read_len < 0) {
    FILE_LOG(LOG_ERROR) << "ndnfs_read: read error. Errno: " << errno << endl;
    return -errno;
  }
  
  close(fd);
  return read_len;
}


int ndnfs_write (const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
  FILE_LOG(LOG_DEBUG) << "ndnfs_write: path=" << path << std::dec << ", size=" << size << ", offset=" << offset << endl;
  
  // First check if the entry exists in the database
  /*sqlite3_stmt *stmt;
  sqlite3_prepare_v2 (db, "SELECT current_version FROM file_system WHERE path = ?;", -1, &stmt, 0);
  sqlite3_bind_text (stmt, 1, path, -1, SQLITE_STATIC);
  int res = sqlite3_step (stmt);
  if (res != SQLITE_ROW) {
    sqlite3_finalize (stmt);
    return -ENOENT;
  }
  
  sqlite3_finalize (stmt); */
  // I am trying to use the xattr created in the mknod.
  
  // Then write read from the actual file
  char full_path[PATH_MAX];
  abs_path(full_path, path);
  char versionNum[PATH_MAX];
  //char* current_versionName = "user.current_version";
  int bufferLength = getxattr(full_path, current_versionName, versionNum, sizeof(versionNum));
  
  if (bufferLength < 0 ){
     FILE_LOG(LOG_ERROR) << "ndnfs_read: ext attribute open rror: Errno: " << errno << endl;
     return -ENOENT;
  } else {
     FILE_LOG(LOG_DEBUG) << "ndnfs_read: ext attribute open success: version Number :" << versionNum << endl;
  }  
  //I just check whether metadata entry exists live the comment above. Code below will continue to be implemented if neccessary 
  /*
  char current_version[bufferLength];
  getxattr(full_path, current_versionName, current_version, sizeof(current_version));*/
    

  int fd = open(full_path, O_RDWR);
  if (fd == -1) {
    FILE_LOG(LOG_ERROR) << "ndnfs_write: open error. Errno: " << errno << endl;
    return -errno;
  }

  int write_len = pwrite(fd, buf, size, offset);
  if (write_len < 0) {
    FILE_LOG(LOG_ERROR) << "ndnfs_write: write error. Errno: " << errno << endl;
    return -errno;
  }
  
  close(fd);
  
  return write_len;  // return the number of bytes written on success
}


int ndnfs_truncate (const char *path, off_t length)
{
  int res = 0;//Additional res 
  // First we check if the entry exists in database
  /*sqlite3_stmt *stmt;
  sqlite3_prepare_v2 (db, "SELECT current_version FROM file_system WHERE path = ?;", -1, &stmt, 0);
  sqlite3_bind_text (stmt, 1, path, -1, SQLITE_STATIC);
  int res = sqlite3_step (stmt);
  if (res != SQLITE_ROW) {
    sqlite3_finalize (stmt);
    return -ENOENT;
  } 
  sqlite3_finalize (stmt);*/
  
  // Then we truncate the actual file
  // I am trying to use the xattr object created in the mknod.
  char full_path[PATH_MAX];
  abs_path(full_path, path);
  char versionNum[PATH_MAX];
  //char* current_versionName = "user.current_version";
  int bufferLength = getxattr(full_path, current_versionName, versionNum, sizeof(versionNum));
  
  if (bufferLength < 0 ){
     FILE_LOG(LOG_ERROR) << "ndnfs_truncate: ext attribute open rror: Errno: " << errno << endl;
     return -ENOENT;
  } else {
     FILE_LOG(LOG_DEBUG) << "ndnfs_truncate: ext attribute open success: " << versionNum << endl;
  }

  //I just check whether metadata entry exists live the comment above. Code below will continue to be implemented if neccessary 
  /*
  char current_version[bufferLength];
  getxattr(full_path, current_versionName, current_version, sizeof(current_version));*/



  
  int trunc_ret = truncate(full_path, length);
  if (trunc_ret == -1) {
    FILE_LOG(LOG_ERROR) << "ndnfs_truncate: error. Full path " << full_path << ". Errno " << errno << endl;
    return -errno;
  }
  
  return res;
}

char* toCstr(int x){
   std::string versionIntString = std::to_string(x);
   char* verValue = (char*)versionIntString.c_str();  
   return verValue;
}

int ndnfs_unlink(const char *path)//This will be rewritten later as to the different scheme.
{
  FILE_LOG(LOG_DEBUG) << "ndnfs_unlink: path=" << path << endl;

  // Now metadata is in-file
  // TODO: update remove_versions
  remove_file_entry(path);

  // Then, remove file entry
/*
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2(db, "DELETE FROM file_system WHERE path = ?;", -1, &stmt, 0);
  sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);*/
  
  char full_path[PATH_MAX];
  abs_path(full_path, path);


  int ret = unlink(full_path);
  
  if (ret == -1) {
    FILE_LOG(LOG_ERROR) << "ndnfs_unlink: unlink failed. Errno: " << errno << endl;
    return -errno;
  }
    
  return 0;
}



int ndnfs_release (const char *path, struct fuse_file_info *fi)
{
  struct tm * begin;
  struct tm * end;
  std::time_t nowTime;
  std::time_t endTime;
  std::chrono::system_clock::time_point started;
  std::chrono::duration<double> sec;

  nowTime = std::time(0);
  begin = localtime( & nowTime );
  started = std::chrono::system_clock::now();
  
  FILE_LOG(LOG_DEBUG) << "ndnfs_release: path=" << path << ", flag=0x" << std::hex << fi->flags << endl;
  FILE_LOG(LOG_DEBUG) << "ndnfs_release start time: " << begin->tm_min << " min, " << begin->tm_sec << " sec " << endl;
  int curr_version = time(0);
  int res = 0;
  // First we check if the file exists
  // Then write the actual file
  // I am trying to use the JSON metadata object created in the mknod.
  
  char full_path[PATH_MAX];
  abs_path(full_path, path);
  char versionNum[PATH_MAX];
  //char* current_versionName = "user.current_version";
  int bufferLength = getxattr(full_path, current_versionName, versionNum, sizeof(versionNum));
  
  if (bufferLength < 0 ){
     FILE_LOG(LOG_ERROR) << "ndnfs_release: ext attribute open rror: Errno: " << errno << endl;
     return -ENOENT;
  } else {
      FILE_LOG(LOG_DEBUG) << "ndnfs_release: ext attribute open success: versionNum : " << versionNum << endl;
  }

  //I just check whether metadata entry exists live the comment above. Code below will continue to be implemented if neccessary 
  /*
  char current_version[bufferLength];
  getxattr(full_path, current_versionName, current_version, sizeof(current_version));*/

  //Version update is going to be implemented. 
  int retSuccess = removexattr(full_path, current_versionName);
  if (retSuccess == -1){
	FILE_LOG(LOG_ERROR) << "ndnfs_release : removing ext-attribute version failed : " << endl;
        return -ENOENT;
  } else {
       FILE_LOG(LOG_DEBUG) << "ndnfs_release: removing ext-attribute success: Curr_versionNanme : " << current_versionName << endl;
  }

  std::string versionString = std::to_string(curr_version);
  char* curr_verValue = (char*)versionString.c_str();


  retSuccess = setxattr(full_path, current_versionName, curr_verValue, strlen(curr_verValue), 0);
  if(retSuccess == -1){
	FILE_LOG(LOG_ERROR) << "ndnfs_release : setting ext-attribute version failed : " << endl;
        return -ENOENT;
  } else {
        FILE_LOG(LOG_DEBUG) << "ndnfs_release: set ext-attribute success: Curr_version : " << curr_verValue << ", "<< "string length : "<< strlen(curr_verValue) <<endl;
  }
  //Version update code.
  //Check whether it has been added well.
  char versionNum1[PATH_MAX];
  retSuccess = getxattr(full_path, current_versionName, versionNum1, sizeof(versionNum1));
  if(retSuccess == -1){
	FILE_LOG(LOG_ERROR) << "ndnfs_release : getting updated ext-attribute version failed : " << endl;
        return -ENOENT;
  } else {
        FILE_LOG(LOG_DEBUG) << "ndnfs_release: get updated ext-attribute success: Curr_version : " << versionNum1 << ", "<< "its sizeof : "<< sizeof(versionNum1) <<endl;
  }
  //sqlite3_stmt *stmt;
  /*sqlite3_prepare_v2 (db, "SELECT current_version FROM file_system WHERE path = ?;", -1, &stmt, 0);
  sqlite3_bind_text (stmt, 1, path, -1, SQLITE_STATIC);
  int res = sqlite3_step (stmt);
  if (res != SQLITE_ROW) {
    sqlite3_finalize (stmt);
    return -ENOENT;
  }
  sqlite3_finalize (stmt);*/
        
 /*
  if ((fi->flags & O_ACCMODE) != O_RDONLY) {
    sqlite3_prepare_v2 (db, "UPDATE file_system SET current_version = ? WHERE path = ?;", -1, &stmt, 0);
    sqlite3_bind_int (stmt, 1, curr_version);  // set current_version to the current timestamp
    sqlite3_bind_text (stmt, 2, path, -1, SQLITE_STATIC);
    res = sqlite3_step (stmt);
    if (res != SQLITE_OK && res != SQLITE_DONE) {
      FILE_LOG(LOG_ERROR) << "ndnfs_release: update file_system error. " << res << endl;
      return res;
    }
    sqlite3_finalize (stmt);
    
    sqlite3_prepare_v2 (db, "INSERT INTO file_versions (path, version) VALUES (?,?);", -1, &stmt, 0);
    sqlite3_bind_text (stmt, 1, path, -1, SQLITE_STATIC);
    sqlite3_bind_int (stmt, 2, curr_version);
    sqlite3_step (stmt);
    sqlite3_finalize (stmt);
    
    // TODO: since older version is removed anyway, it makes sense to rely on system 
    // function calls for multiple file accesses. Simplification of versioning method?
    //if (curr_ver != -1)
    //  remove_version (path, curr_ver);
    
    // After releasing, start a new signing thread for the file; 
    // If a signing thread for the file in question has already started, kill that thread.
    char full_path[PATH_MAX];
    abs_path(full_path, path);
  
    int fd = open(full_path, O_RDONLY);
    
    if (fd == -1) {
      FILE_LOG(LOG_ERROR) << "ndnfs_release: open error. Errno: " << errno << endl;
      return -errno;
    }
    
    char buf[ndnfs::seg_size];
    int size = ndnfs::seg_size;
    int seg = 0;
    
    while (size == ndnfs::seg_size) {
      size = pread(fd, buf, ndnfs::seg_size, seg << ndnfs::seg_size_shift);
      if (size == -1) {
        FILE_LOG(LOG_ERROR) << "ndnfs_release: read error. Errno: " << errno << endl;
	FILE_LOG(LOG_DEBUG) << "ndnfs_release end time: " <<  end->tm_min << " min, " << end->tm_sec << " sec. " << endl;
  	FILE_LOG(LOG_DEBUG) << "ndnfs_release total time elapsed : "<< sec.count() << " sec " << endl;
        return -errno;    
      }
      sign_segment (path, curr_version, seg, buf, size);
      seg ++;
    }
    
    close(fd);
  }*/
  endTime = std::time(0);
  end = localtime( & endTime );
  sec = std::chrono::system_clock::now() - started;
  FILE_LOG(LOG_DEBUG) << "ndnfs_release end time: " <<  end->tm_min << " min, " << end->tm_sec << " sec. " << endl;
  FILE_LOG(LOG_DEBUG) << "ndnfs_release total time elapsed : "<< sec.count() << " sec " << endl;
  return 0;
}

int ndnfs_utimens(const char *path, const struct timespec ts[2])
{        
  int res;
  struct timeval tv[2];
  
  char full_path[PATH_MAX];
  abs_path(full_path, path);
  
  tv[0].tv_sec = ts[0].tv_sec;
  tv[0].tv_usec = ts[0].tv_nsec / 1000;
  tv[1].tv_sec = ts[1].tv_sec;
  tv[1].tv_usec = ts[1].tv_nsec / 1000;

  res = utimes(full_path, tv);
  if (res == -1)
    return -errno;

  return 0;
}

int ndnfs_readlink(const char *path, char *buf, size_t size)
{
  int res;
  
  char full_path[PATH_MAX];
  abs_path(full_path, path);
  
  res = readlink(full_path, buf, size - 1);
  if (res == -1)
	return -errno;

  buf[res] = '\0';
  return 0;
}

/**
 * symlink handling inserts file and version entry for the symlink name, 
 * but does not create segments entry;
 * TODO: file_segments entries should be linked to another name in file_system;
 * Symlink, as well as hard links will not be available for remote fetching.
 */
int ndnfs_symlink(const char *from, const char *to)
{
  int res;
  
  /*
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2(db, "SELECT * FROM file_system WHERE path = ?;", -1, &stmt, 0);
  sqlite3_bind_text(stmt, 1, to, -1, SQLITE_STATIC);
  res = sqlite3_step(stmt);
  if (res == SQLITE_ROW) {
      // Cannot create symlink that has conflicting file name
      sqlite3_finalize(stmt);
      return -ENOENT;
  }
  
  sqlite3_finalize(stmt);
  
  // Generate first version entry for the new symlink
  int ver = time(0);
  
  sqlite3_prepare_v2(db, "INSERT INTO file_versions (path, version) VALUES (?, ?);", -1, &stmt, 0);
  sqlite3_bind_text(stmt, 1, to, -1, SQLITE_STATIC);
  sqlite3_bind_int(stmt, 2, ver);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  // Add the symlink entry to database
  sqlite3_prepare_v2(db, 
                     "INSERT INTO file_system \
                      (path, current_version, mime_type, type) \
                      VALUES (?, ?, ?, ?);", 
                     -1, &stmt, 0);
  sqlite3_bind_text(stmt, 1, to, -1, SQLITE_STATIC);
  sqlite3_bind_int(stmt, 2, ver);  // current version
  sqlite3_bind_text(stmt, 3, "", -1, SQLITE_STATIC);
  sqlite3_bind_int(stmt, 4, SYMBOLIC_LINK);
  
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);  
  */
  
  char full_path_from[PATH_MAX];
  abs_path(full_path_from, from);
  
  char full_path_to[PATH_MAX];
  abs_path(full_path_to, to);
  
  res = symlink(full_path_from, full_path_to);
  if (res == -1)
	return -errno;

  return 0;
}

/**
 * Link is called on the creation of hard links
 * TODO: file_segments entries should be linked to another name in file_system;
 * This is not implemented and hard links will not be available for remote fetching.
 */
int ndnfs_link(const char *from, const char *to)
{
  int res;
  
  // actual linking of paths
  char full_path_from[PATH_MAX];
  abs_path(full_path_from, from);
  
  char full_path_to[PATH_MAX];
  abs_path(full_path_to, to);
  
  res = link(full_path_from, full_path_to);
  if (res == -1)
	return -errno;

  return 0;
}

/**
 * Right now, rename changes every entry related with the content, without creating new version
 * TODO: Rename would require checking if rename target (avoid collision error in db) already exists, and resigning of everything...
 * Rename should better work as a duplicate.
 */
int ndnfs_rename(const char *from, const char *to)//Not implemented yet because it is not associated with file write operations.
{
  int res = 0;
  /*sqlite3_stmt *stmt;
  
  sqlite3_prepare_v2(db, "UPDATE file_system SET PATH = ? WHERE path = ?;", -1, &stmt, 0);
  sqlite3_bind_text(stmt, 1, to, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, from, -1, SQLITE_STATIC);
  sqlite3_step(stmt);

  if (res != SQLITE_OK && res != SQLITE_DONE) {
	FILE_LOG(LOG_ERROR) << "ndnfs_rename: update file_system error. " << res << endl;
	return res;
  }
  sqlite3_finalize (stmt);

  sqlite3_prepare_v2(db, "UPDATE file_versions SET PATH = ? WHERE path = ?;", -1, &stmt, 0);
  sqlite3_bind_text(stmt, 1, to, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, from, -1, SQLITE_STATIC);
  sqlite3_step(stmt);

  if (res != SQLITE_OK && res != SQLITE_DONE) {
	FILE_LOG(LOG_ERROR) << "ndnfs_rename: update file_versions error. " << res << endl;
	return res;
  }
  sqlite3_finalize (stmt);

  sqlite3_prepare_v2(db, "UPDATE file_segments SET PATH = ? WHERE path = ?;", -1, &stmt, 0);
  sqlite3_bind_text(stmt, 1, to, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, from, -1, SQLITE_STATIC);
  sqlite3_step(stmt);

  if (res != SQLITE_OK && res != SQLITE_DONE) {
	FILE_LOG(LOG_ERROR) << "ndnfs_rename: update file_segments error. " << res << endl;
	return res;
  }
  sqlite3_finalize (stmt);*/
  
  //Path attribute of setxattr will be changed to "full path to"
  // actual renaming
  char full_path_from[PATH_MAX];
  abs_path(full_path_from, from);
  
  char full_path_to[PATH_MAX];
  abs_path(full_path_to, to);
  char* nameTo=(char*)(to);

  FILE_LOG(LOG_DEBUG) << "ndnfs_rename: full path from " << full_path_from << ", "<< " full path to : "<< full_path_to <<endl;

  int retSuccess = removexattr(full_path_from, pathName);
  if (retSuccess == -1){
  	FILE_LOG(LOG_ERROR) << "ndnfs_rename : removing ext-attribute path failed : " << endl;
	  return -errno;
        	
  } else {
          FILE_LOG(LOG_DEBUG) << "ndnfs_rename : removing ext-attribute path success: " << to << endl;
	  FILE_LOG(LOG_DEBUG) << "ndnfs_rename : attribute: pathName" << pathName<<" attribute full path from : " <<full_path_from << endl;
		//int rc1 = setxattr(full_path_to, pathName, nameTo, strlen(nameTo), 0);
  }	
 	 
  int rtt1 = setxattr(full_path_from, pathName, nameTo, strlen(nameTo), 0);
  if(rtt1 == -1){
   FILE_LOG(LOG_ERROR) <<  "ndnfs_rename : setting new ext-attribute path failed: "<< endl;
   FILE_LOG(LOG_ERROR) <<  "ndnfs_rename : full_path : "<< full_path_from<<" attribute name : "<<pathName<<"nameTo : "<<nameTo<< endl;
   return -errno;
  }

  
  res = rename(full_path_from, full_path_to);
  if(res == -1){
  	FILE_LOG(LOG_ERROR) << "ndnfs_rename: rename has been failed, no xattr feature will be run." << endl;
  } else {
       	 FILE_LOG(LOG_ERROR) << "ndnfs_rename: rename has been success, so far so good" << endl;

	 
  }
  
  FILE_LOG(LOG_ERROR) << "ndnfs_rename: rename should trigger resign of everything, which is not yet implemented" << endl;
  if (res == -1)
	return -errno;

  return 0;
}

int ndnfs_statfs(const char *path, struct statvfs *si)
{
  char full_path[PATH_MAX];
  abs_path(full_path, path);
  
  int ret = statvfs(full_path, si);

  if (ret == -1) {
    FILE_LOG(LOG_ERROR) << "ndnfs_statfs: stat failed. Errno " << errno << endl;
    return -errno;
  }
  
  return 0;
}

int ndnfs_access(const char *path, int mask)
{
  char full_path[PATH_MAX];
  abs_path(full_path, path);
  
  int ret = access(full_path, mask);

  if (ret == -1) {
    FILE_LOG(LOG_ERROR) << "ndnfs_access: access failed. Errno " << errno << endl;
    return -errno;
  }
  
  return 0;
}

