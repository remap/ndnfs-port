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

#include "segment.h"
#include <ndn-cpp/data.hpp>
#include <ndn-cpp/common.hpp>
#include <ndn-cpp/security/security-exception.hpp>

#include <iostream>
#include <cstdio>

#define INT2STRLEN 100

using namespace std;
using namespace ndn;

/**
 * version parameter is not used right now, as duplicate_version is now a stub, 
 * and older versions of the file are not actually stored or accessible.
 */
int read_segment(const char* path, const int ver, const int seg, char *output, const int limit, const int offset, struct fuse_file_info *fi)
{
#ifdef NDNFS_DEBUG
  cout << "read_segment: path=" << path << std::dec << ", ver=" << ver << ", seg=" << seg << ", limit=" << limit << ", offset=" << offset << endl;
#endif
  char *temp = new char[ndnfs::seg_size];
  int read_len = pread(fi->fh, temp, ndnfs::seg_size, segment_to_size(seg) + offset);
  
  if (read_len < 0) {
    cerr << "read_segment: read error." << endl;
    return -1;
  }
  
  if (read_len > limit)  // Don't write across the limit
	read_len = limit;
  
  memcpy(output, temp, read_len);
  delete temp;
  
  return read_len;
}

/**
 * version parameter is not used right now, as duplicate_version is now a stub, 
 * and write does not create/write to a new file by the name of the version.
 */
int write_segment(const char* path, const int ver, const int seg, const char *data, const int len, struct fuse_file_info *fi)
{
#ifdef NDNFS_DEBUG
  cout << "write_segment: path=" << path << std::dec << ", ver=" << ver << ", seg=" << seg << ", len=" << len << endl;
#endif

  assert(len > 0);

  string file_path(path);
  string full_name = ndnfs::global_prefix + file_path;
  // We want the Name(uri) constructor to split the path into components between "/", but we first need
  // to escape the characters in full_name which the Name(uri) constructor will unescape.  So, create a component
  // from the raw string and use its toEscapedString.
  
  string escapedString = Name::Component((uint8_t*)&full_name[0], full_name.size()).toEscapedString();
  // The "/" was escaped, so unescape.
  while(1) {
	size_t found = escapedString.find("%2F");
	if (found == string::npos) break;
	escapedString.replace(found, 3, "/");
  }
  Name seg_name(escapedString);
  
  seg_name.appendVersion(ver);
  seg_name.appendSegment(seg);
#ifdef NDNFS_DEBUG
  cout << "write_segment: segment name is " << seg_name.toUri() << endl;
#endif

  Data data0;
  data0.setName(seg_name);
  data0.setContent((const uint8_t*)data, len);
  //data0.getMetaInfo().setTimestampMilliseconds(time(NULL) * 1000.0);
  
  // instead of putting the whole content object into sqlite, we put only the signature field.
  ndnfs::keyChain->sign(data0, ndnfs::certificateName);
  Blob signature = data0.getSignature()->getSignature();
  
  const char* sig_raw = (const char*)signature.buf();
  int sig_size = strlen(sig_raw);

#ifdef NDNFS_DEBUG
  cout << "write_segment: raw signature is" << endl;
  for (int i = 0; i < sig_size; i++) {
	printf("%02x", (unsigned char)sig_raw[i]);
  }
  cout << endl;
  cout << "write_segment: raw signature length is " << sig_size << endl;
#endif

  sqlite3_stmt *stmt;
  sqlite3_prepare_v2(db, "INSERT OR REPLACE INTO file_segments (path,version,segment,signature,offset) VALUES (?,?,?,?,?);", -1, &stmt, 0);
  sqlite3_bind_text(stmt,1,path,-1,SQLITE_STATIC);
  sqlite3_bind_int(stmt,2,ver);
  sqlite3_bind_int(stmt,3,seg);
  
  sqlite3_bind_blob(stmt,4,sig_raw,sig_size,SQLITE_STATIC);
  
  sqlite3_bind_int(stmt,5,segment_to_size(seg));
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  
  // the actual writing of the file content.
  int write_len = pwrite(fi->fh, data, len, segment_to_size(seg));
  if (write_len < 0) {
    cerr << "write_segment: write error." << endl;
    return -1;
  }
  
  return write_len;
}

void remove_segments(const char* path, const int ver, const int start/* = 0 */)
{
#ifdef NDNFS_DEBUG
    cout << "remove_segments: path=" << path << std::dec << ", ver=" << ver << ", starting from segment #" << start << endl;
#endif

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "SELECT totalSegments FROM file_versions WHERE path = ? AND version = ?;", -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, ver);
    int res = sqlite3_step(stmt);
    if (res != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return;
    }
    int segs = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    for (int i = start; i < segs; i++) {
        sqlite3_prepare_v2(db, "DELETE FROM file_segments WHERE path = ? AND version = ? AND segment = ?;", -1, &stmt, 0);
        sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, ver);
        sqlite3_bind_int(stmt, 3, i);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

void truncate_segment(const char* path, const int ver, const int seg, const off_t length)
{
#ifdef NDNFS_DEBUG
    cout << "truncate_segment: path=" << path << std::dec << ", ver=" << ver << ", seg=" << seg << ", length=" << length << endl;
#endif

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "SELECT * FROM file_segments WHERE path = ? AND version = ? AND segment = ?;", -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, ver);
    sqlite3_bind_int(stmt, 3, seg);
    if(sqlite3_step(stmt) == SQLITE_ROW) {
        if (length == 0) {
            sqlite3_finalize(stmt);
            sqlite3_prepare_v2(db, "DELETE FROM file_segments WHERE path = ? AND version = ? AND segment = ?;", -1, &stmt, 0);
            sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 2, ver);
            sqlite3_bind_int(stmt, 3, seg);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        } else {
            const char* co_raw = (const char *)sqlite3_column_blob(stmt, 3);
            int co_size = sqlite3_column_bytes(stmt, 3);

            assert(co_size > (int)length);
            
            Data data;
            data.wireDecode((const uint8_t*)co_raw, co_size);
            const uint8_t *content = data.getContent().buf();

            Data trunc_data;
            trunc_data.setName(data.getName());
            trunc_data.setContent(content, length);
            //trunc_data.getMetaInfo().setTimestampMilliseconds(time(NULL) * 1000.0);
            ndnfs::keyChain->sign(trunc_data, ndnfs::certificateName);
            SignedBlob wire_data = trunc_data.wireEncode();
            const uint8_t *trunc_co_raw = wire_data.buf();
            int trunc_co_size = wire_data.size();

            sqlite3_finalize(stmt);
            sqlite3_prepare_v2(db, "UPDATE file_segments SET data = ? WHERE path = ? AND version = ? AND segment = ?;", -1, &stmt, 0);
            sqlite3_bind_blob(stmt, 1, trunc_co_raw, trunc_co_size, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, path, -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 3, ver);
            sqlite3_bind_int(stmt, 4, seg);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }
}
