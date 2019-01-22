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

#include "version.h"

#include <ndn-cpp/data.hpp>
#include <ndn-cpp/common.hpp>

using namespace std;
using namespace ndn;

// duplicate_version right now is a stub
int duplicate_version (const char *path, const int from_ver, const int to_ver)
{
  FILE_LOG(LOG_DEBUG) << "duplicate_version need to be reimplemented." << endl;
  return 0;
}

// write_version's function will be redefined
int write_version(const char* path, int ver, const char *buf, size_t size, off_t offset)
{
  FILE_LOG(LOG_ERROR) << "write_version need to be reimplemented." << endl;
  return 0;
}

int truncate_version(const char* path, const int ver, off_t length)
{
  FILE_LOG(LOG_DEBUG) << "truncate_version: path=" << path << std::dec << ", ver=" << ver << ", length=" << length << endl;

  sqlite3_stmt *stmt;
  sqlite3_prepare_v2 (db, "SELECT * FROM file_versions WHERE path = ? AND version = ?;", -1, &stmt, 0);
  sqlite3_bind_text (stmt, 1, path, -1, SQLITE_STATIC);
  sqlite3_bind_int (stmt, 2, ver);

  if (sqlite3_step (stmt) != SQLITE_ROW) {
    // Should not happen
    sqlite3_finalize (stmt);
    return -1;
  }
  
  int size = sqlite3_column_int (stmt, 2);
  sqlite3_finalize (stmt);
  
  if ((size_t) length == size) {
    return 0;
  }
  else if ((size_t) length < size) {
    // Truncate to length
    int seg_end = seek_segment (length);

    sqlite3_prepare_v2 (db, "UPDATE file_versions SET size = ?, totalSegments = ? WHERE path = ? and version = ?;", -1, &stmt, 0);
    sqlite3_bind_int (stmt, 1, (int) length);
    sqlite3_bind_int (stmt, 2, seg_end);
    sqlite3_bind_text (stmt, 3, path, -1, SQLITE_STATIC);
    sqlite3_bind_int (stmt, 4, ver);
    int res = sqlite3_step (stmt);
    sqlite3_finalize (stmt);
    if (res != SQLITE_OK && res != SQLITE_DONE)
      return -1;

    // Update version size and segment list
    int tail = length - segment_to_size (seg_end);
    
    truncate_segment (path, ver, seg_end, tail);
    remove_segments (path, ver, seg_end + 1);
    
    return 0;
  }
  else {
    // TODO: pad with zeros
    return -1;
  }
}

void remove_version(const char* path, const int ver)
{
  FILE_LOG(LOG_DEBUG) << "remove_version: path=" << path << ", ver=" << std::dec << ver << endl;

  remove_segments(path, ver);
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2(db, "DELETE FROM file_versions WHERE path = ? and version = ?;", -1, &stmt, 0);
  sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
  sqlite3_bind_int(stmt, 2, ver);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
}

void remove_file_entry(const char* path)
{
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2(db, "DELETE FROM file_system WHERE path = ?;", -1, &stmt, 0);
  sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
}
