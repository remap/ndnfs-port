/*
 * Copyright (c) 2014 University of California, Los Angeles
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
 * This is the ported ndnfs(https://github.com/wentaoshang/NDNFS.git)
 * to ndn-cpp(CCL) and NFD.
 *
 * Author: Zhehao Wang, based on the work of
 * Qiuhan Ding <dingqiuhan@gmail.com>, Wentao Shang <wentao@cs.ucla.edu>
 */
 
#include <cstdio>
#include <iostream>
#include <sstream>
#include <vector>
#include <fcntl.h>
#include <dirent.h>

#include "servermodule.h"
#include <ndn-cpp/face.hpp>
#include <ndn-cpp/interest.hpp>
#include <ndn-cpp/security/key-chain.hpp>
#include <ndn-cpp/common.hpp>

#include <ndn-cpp/digest-sha256-signature.hpp>
#include <ndn-cpp/sha256-with-ecdsa-signature.hpp>
#include <ndn-cpp/sha256-with-rsa-signature.hpp>

#include <sys/stat.h>

using namespace std;
using namespace ndn;
using namespace ndnfs::server;

/**
 * readFileSize reads a file from path, and extracts its size and number of segments.
 * @param path String path to the file
 * @param file_size Overwritten with number of bytes of the file
 * @param total_seg Overwritten with number of segments of the file
 */
void readFileSize(string path, int& file_size, int& total_seg)
{
  char file_path[PATH_MAX] = "";
  abs_path(file_path, path.c_str());
  
  struct stat st;
  stat(file_path, &st);
  file_size = st.st_size;
  total_seg = (file_size >> SEGSIZESHIFT) + 1;
  return;
}

void onInterest(const ptr_lib::shared_ptr<const Name>& prefix, const ptr_lib::shared_ptr<const Interest>& interest, Transport& transport, uint64_t registeredPrefixId) 
{
  processInterest(interest->getName(), transport);
}

void onRegisterFailed(const ptr_lib::shared_ptr<const Name>& prefix) 
{
  cerr << "Register failed" << endl;
}

int parseName(const ndn::Name& name, int &version, int &seg, string &path) 
{
  int ret = -1;
  version = -1;
  seg = -1;
  
  // this should be changed to using toVersion, not using the the octets directly in case
  // of future changes in naming conventions...
  ndn::Name::const_iterator iter = name.begin();
  ostringstream oss;
  
  for (; iter != name.end(); iter++) {
	const uint8_t marker = *(iter->getValue().buf());
	
	if (marker == 0xFD) {
	  // Right now, having two versions does not make sense.
	  if (version == -1) {
		version = iter->toVersion();
	  }
	  else {
		return -1;
	  }
	}
	else if (marker == 0x00) {
	  // Having segment number before version number does not make sense
	  if (version == -1) {
		return -1;
	  }
	  // Having two segment numbers does not make sense
	  else if (seg != -1) {
		return -1;
	  }
	  else {
		seg = iter->toSegment();
	  }
	}
	else if (marker == 0xC1) {
	  continue;
	}
	else {
	  string component = iter->toEscapedString();
	  
	  // Deciding if this interest is asking for meta_info
	  // If the component comes after <version> but not <segment>, it is interpreted 
	  // as either a meta request, or an invalid interest.
	  if (version != -1 && seg == -1) {
		if (component == NdnfsNamespace::metaComponentName_) {
		  ret = 4;
		}
		else {
		  return -1;
		}
	  }
	  // If the component comes before <version> and <segment>, it is interpreted as
	  // part of the path.
	  else if (version == -1 && seg == -1) {
		oss << "/" << component;
	  }
	  // If the component comes after <version>/<segment>, it is considered invalid
	  else {
		return -1;
	  }
	}
  }
  
  // we scanned through a valid interest name
  path = oss.str();

  path = path.substr(fs_prefix.length());
  if (path == "")
	  path = string("/");
	 
  // has <version>/<segment> 
  if (version != -1 && seg != -1) {
	  ret = 3;
  }
  // has <version>, but not _meta
  else if (version != -1 && ret != 4) {
	  ret = 2;
  }
  // has nothing
  else if (version == -1) {
	  ret = 1;
  }
  return ret;
}

void processInterest(const Name& interest_name, Transport& transport) 
{
  string path;
  int version;
  int seg;
  int ret = parseName(interest_name, version, seg, path);

  // The client is asking for a segment of a file.
  if (ret == 3) {
	ret = sendFileContent(interest_name, path, version, seg, transport);
    if (ret != -1) {
      
    }
  }
  // The client is asking for a certain version of a file
  else if (ret == 2) {
	// TODO: when asking for file with version specified, current server does not query for the mime_type
	ret = sendFileAttr(path, "", version, transport);
	if (ret == -1) {
	  cout << "processInterest(): no such file/version found in ndnfs: " << path << endl;
	  return ;
	}
  }
  // The client is asking for 'generic' info about a file/folder in ndnfs
  else if (ret == 1) {
	sqlite3_stmt *stmt;
	sqlite3_prepare_v2(db, "SELECT current_version, mime_type FROM file_system WHERE path = ?", -1, &stmt, 0);
	sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_STATIC);
	if (sqlite3_step(stmt) != SQLITE_ROW) {
	  cout << "processInterest(): no such file found in ndnfs: " << path << endl;
	  sqlite3_finalize(stmt);
	  
	  // It may not be a file, but a folder instead, which is not stored in database
	  ret = sendDirAttr(path, transport);
	}
	else {
	  int ver = sqlite3_column_int(stmt, 0);
	  string mimeType = string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)));
	
	  sqlite3_finalize(stmt);
	  ret = sendFileAttr(path, mimeType, ver, transport);
    }
    return;
  }
  // The client is asking for the meta_info of a file
  else if (ret == 4) {
    ret = sendFileMeta(interest_name, path, version, transport);
    return;
  }
}

int sendFileMeta(Name interest_name, string path, int version, Transport& transport)
{
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2(db, "SELECT mime_type FROM file_system WHERE path = ?", -1, &stmt, 0);
  sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_STATIC);
  if (sqlite3_step(stmt) != SQLITE_ROW) {
	cout << "processInterest(): no such file/directory found in ndnfs: " << path << endl;
	sqlite3_finalize(stmt);
	return -1;
  }
  
  // right now, if the requested path is a file, whenever meta_info is asked, the server replies with 
  // name: <received name>/<mime_type>, content: mime_type	
  // if the requested path is a folder, the server does not reply with anything	
  string mimeType = string((char *)sqlite3_column_text(stmt, 1));
  
  version = sqlite3_column_int(stmt, 7);
  sqlite3_finalize(stmt);
  sqlite3_prepare_v2(db, "SELECT * FROM file_versions WHERE path = ? AND version = ? ", -1, &stmt, 0);
  sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_int(stmt, 2, version);
  
  if (sqlite3_step(stmt) != SQLITE_ROW) {
	sqlite3_finalize(stmt);
	return -1;
  }
	
  Name name(interest_name);
  name.append(NdnfsNamespace::mimeComponentName_);
  
  Data data(name);
  data.setContent((const uint8_t *)&mimeType[0], mimeType.size());
  keyChain->sign(data, certificateName);
  transport.send(*data.wireEncode());
  
  sqlite3_finalize(stmt);
  return 0;
}

int sendFileContent(Name interest_name, string path, int version, int seg, Transport& transport)
{
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2(db, "SELECT * FROM file_segments WHERE path = ? AND version = ? AND segment = ?", -1, &stmt, 0);
  sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_int(stmt, 2, version);
  sqlite3_bind_int(stmt, 3, seg);
  if(sqlite3_step(stmt) != SQLITE_ROW){
	cout << "processInterest(): no such file version/segment found in ndnfs: " << path << endl;
	sqlite3_finalize(stmt);
	return -1;
  }

  const char * signatureBlob = (const char *)sqlite3_column_blob(stmt, 3);
  int len = sqlite3_column_bytes(stmt, 3);
  sqlite3_finalize(stmt);

  // For now, the signature type is assumed to be Sha256withRSA; should read
  // signature type from database, which is not yet implemented in the database
  Sha256WithRsaSignature signature;
  
  signature.setSignature(Blob((const uint8_t *)signatureBlob, len));
  
  Data data;
  data.setName(interest_name);
  data.setSignature(signature);

  // When assembling the data packet, finalblockid should be put into each segment,
  // this means when reading each segment, file_version also needs to be consulted for the finalBlockId.
  int total_seg = 0;
  int file_size = 0;
  readFileSize(path, file_size, total_seg);

  if (total_seg > 0) {
	// in the JS plugin, finalBlockId component is parsed with toSegment
	// not sure if it's supposed to be like this in other ndn applications,
	// if so, should consider adding wrapper in library 'toSegment' (returns Component), instead of just 'appendSegment' (returns Name)
	Name::Component finalBlockId = Name::Component::fromNumberWithMarker(total_seg - 1, 0x00);
	data.getMetaInfo().setFinalBlockId(finalBlockId);
  }
  
  int fd;
  char file_path[PATH_MAX] = "";
  abs_path(file_path, path.c_str());
  
  fd = open(file_path, O_RDONLY);
  
  if (fd == -1) {
	cout << "Open " << file_path << " failed." << endl;
	return -1;
  }
  
  char output[BLOCKSIZE] = "";
  int actual_len = pread(fd, output, BLOCKSIZE, seg << SEGSIZESHIFT);
  
  close(fd);
  
  if (actual_len == -1) {
	cout << "Read from " << file_path << " failed." << endl;
	return -1;
  }
  
  data.setContent((uint8_t*)output, actual_len);
  
  Blob encodedData = data.wireEncode();
  transport.send(*encodedData);
  return actual_len;
}

int sendFileAttr(const string& path, const string& mimeType, int version, Transport& transport) 
{
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2(db, "SELECT * FROM file_versions WHERE path = ? AND version = ? ", -1, &stmt, 0);
  sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_int(stmt, 2, version);
  if (sqlite3_step(stmt) != SQLITE_ROW){
	sqlite3_finalize(stmt);
	return -1;
  }
  sqlite3_finalize(stmt);
  
  int total_seg = 0;
  int file_size = 0;
  readFileSize(path, file_size, total_seg);
  
  ndnfs::FileInfo infof;
  
  infof.set_size(file_size);
  infof.set_totalseg(total_seg);
  infof.set_version(version);
  
  if (mimeType != "") {
	infof.set_mimetype(mimeType);
  }
  
  char *wireData = new char[infof.ByteSize()];
  infof.SerializeToArray(wireData, infof.ByteSize());
  Name name(fs_prefix);
  name.append(Name(path));
  
  Blob ndnfsFileComponent = Name::fromEscapedString(NdnfsNamespace::fileComponentName_);
  name.append(ndnfsFileComponent).appendVersion(version);
  Data data;
  data.setName(name);
  
  Name::Component finalBlockId = Name::Component::fromNumberWithMarker(total_seg - 1, 0x00);
  
  data.getMetaInfo().setFinalBlockId(finalBlockId);
  data.setContent((uint8_t*)wireData, infof.ByteSize());
  
  keyChain->sign(data, certificateName);
  transport.send(*data.wireEncode());
  
  cout << "Data returned with name: " << name.toUri() << endl;
  
  delete wireData;
  return 0;
}

int sendDirAttr(string path, Transport& transport) {
  char dir_path[PATH_MAX] = "";
  abs_path(dir_path, path.c_str());
	
  DIR *dp = opendir(dir_path);
  if (dp == NULL) {
	return -1;
  }
  
  int count = 0;
  ndnfs::DirInfoArray infoa;
  struct dirent *de;
  
  struct stat st;
  lstat(dir_path, &st);
  int mtime = st.st_mtime;
  
  while ((de = readdir(dp)) != NULL) {
	ndnfs::DirInfo *infod = infoa.add_di();
	
	lstat(de->d_name, &st);
	if(S_ISDIR(st.st_mode)) {
	  infod->set_type(dir_type);
	} else {
	  infod->set_type(file_type);
	}
    infod->set_path(de->d_name);
    count ++;
  }
  closedir(dp);
  
  Name name(fs_prefix);
  name.append(Name(path));

  Blob ndnfsDirComponent = Name::fromEscapedString(NdnfsNamespace::dirComponentName_);
  name.append(ndnfsDirComponent).appendVersion(mtime);

  Data data;
  data.setName(name);
  char *wireData;
  int dataSize = 0;
  
  if (count > 0) {
    dataSize = infoa.ByteSize();
	wireData = new char[dataSize];
	infoa.SerializeToArray(wireData, dataSize);
  }
  else {
    dataSize = strlen("Empty folder.\n") + 1;
    wireData = new char[dataSize];
    strcpy(wireData, "Empty folder.\n");
  }
  
  data.setContent((uint8_t*)wireData, dataSize);
  keyChain->sign(data, certificateName);
  transport.send(*data.wireEncode());
  
  delete wireData;
  return 0;
}
