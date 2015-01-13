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

#include "version.h"

#include <ndn-cpp/data.hpp>
#include <ndn-cpp/common.hpp>

using namespace std;
using namespace ndn;

int read_version(const char *path, const int ver, char *output, size_t size, off_t offset, struct fuse_file_info *fi)
{
#ifdef NDNFS_DEBUG
  cout << "read_version: path=" << path << std::dec << ", ver=" << ver << ", size=" << size << ", offset=" << offset << endl;
#endif

  sqlite3_stmt *stmt;
  sqlite3_prepare_v2(db, "SELECT * FROM file_versions WHERE path = ? AND version = ?;", -1, &stmt, 0);
  sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
  sqlite3_bind_int(stmt, 2, ver);

  int res = sqlite3_step(stmt);
  if (res != SQLITE_ROW) {
	sqlite3_finalize(stmt);
	return -1;
  }

  int file_size = sqlite3_column_int(stmt, 2);
  sqlite3_finalize(stmt);

  if (file_size <= (size_t)offset || file_size == 0)
	return 0;

  if (offset + size > file_size) /* Trim the read to the file size. */
	size = file_size - offset;
  
  // find the segment index before the offset position
  int seg = seek_segment(offset);

  // Read first segment starting from some offset
  int total_read = read_segment(path, ver, seg, output, size, (offset - segment_to_size(seg)), fi);
  if (total_read == -1) {
	return 0;
  }
  size -= total_read;
  seg++;

  int seg_read = 0;
  while (size > 0) {
	// Read the rest of the segments starting at zero offset
	seg_read = read_segment(path, ver, seg++, output + total_read, size, 0, fi);
	if (seg_read == -1) {
	  // If anything is wrong when reading segments, just return what we have got already
	  break;
	}
	total_read += seg_read;
	size -= seg_read;
  }

  return total_read;
}

// in current implementation, why is it necessary to 'duplicate' version?
int duplicate_version (const char *path, const int from_ver, const int to_ver)
{
#ifdef NDNFS_DEBUG
  cout << "duplicate_version: path=" << path << std::dec << ", from=" << from_ver << ", to=" << to_ver << endl;
#endif

  sqlite3_stmt *stmt;
  sqlite3_prepare_v2 (db, "SELECT size, totalSegments FROM file_versions WHERE path = ? AND version = ?;", -1, &stmt, 0);
  sqlite3_bind_text (stmt, 1, path, -1, SQLITE_STATIC);
  sqlite3_bind_int (stmt, 2, from_ver);
  if (sqlite3_step (stmt) != SQLITE_ROW) {
	sqlite3_finalize (stmt);
	return -1;
  }
  int ver_size = sqlite3_column_int (stmt, 0);
  int total_seg = sqlite3_column_int (stmt, 1);

  // Insert "to" version entry
  sqlite3_finalize (stmt);
  sqlite3_prepare_v2 (db, "INSERT INTO file_versions (path, version, size, totalSegments) VALUES (?,?,?,?);", -1, &stmt, 0);
  sqlite3_bind_text (stmt, 1, path, -1, SQLITE_STATIC);
  sqlite3_bind_int (stmt, 2, to_ver);
  sqlite3_bind_int (stmt, 3, ver_size);
  sqlite3_bind_int (stmt, 4, total_seg);
  int res = sqlite3_step (stmt);
  sqlite3_finalize (stmt);
  if (res != SQLITE_OK && res != SQLITE_DONE)
    return -1;

  return 0;
}


// Write data to the specified version and return the new file size for that version
int write_version (const char* path, const int ver, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
#ifdef NDNFS_DEBUG
  cout << "write_version: path=" << path << std::dec << ", ver=" << ver << ", size=" << size << ", offset=" << offset << endl;
  //cout << "write_version: content to write is " << endl;
  //for (int i = 0; i < size; i++)
  // {
  //   cout << buf[i];
  // }
  //cout << endl;
#endif
  
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2 (db, "SELECT * FROM file_versions WHERE path = ? AND version = ?;", -1, &stmt, 0);
  sqlite3_bind_text (stmt, 1, path, -1, SQLITE_STATIC);
  sqlite3_bind_int (stmt, 2, ver);
  
  if (sqlite3_step (stmt) != SQLITE_ROW) {
	sqlite3_finalize (stmt);
	return -1;
  }

  int old_ver_size = sqlite3_column_int (stmt, 2);
  int old_total_seg = sqlite3_column_int (stmt, 3);
  const char *buf_pos = buf;
  size_t size_left = size;
  int seg = seek_segment (offset);
  int tail = offset - segment_to_size (seg);
  
  if (tail > 0) {
    // Special handling for the boundary segment: resign this entire segment
	int copy_len = ndnfs::seg_size - tail;  // This is what we can copy at most in this segment
	bool final = false;
	if (copy_len > size) {
	  // The data we want to write may not fill out the rest of the segment
	  copy_len = size;
	  final = true;
	}

	char *new_data = new char[ndnfs::seg_size];
	if (new_data == NULL) {
	  sqlite3_finalize (stmt);
	  return -1;
	}
	
	char *old_data = new char[ndnfs::seg_size];
	int read_len = pread(fi->fh, old_data, ndnfs::seg_size, segment_to_size(seg));
	
	if (read_len == -1) {
	  cerr << "write_version: Write with boundary segment, read from " << path << " failed." << endl;
	  sqlite3_finalize (stmt);
	  return -1;
	}

	memcpy (new_data, old_data, read_len);  // Copy everything from old data
	memcpy (new_data + tail, buf, copy_len);  // Overwrite the part we want to write to

	// The final size of this segment content is the maximum of the old size and the new size
	int updated_seg_len = tail + copy_len;
	updated_seg_len = read_len > updated_seg_len ? read_len : updated_seg_len;
	write_segment (path, ver, seg++, new_data, updated_seg_len, fi);
	
	delete new_data;
	delete old_data;
	  
	if (final) {
	  goto out;
	}	
	// Else, move pointer forward
	buf_pos += copy_len;
	size_left -= copy_len;
  }
    
  while (size_left > 0) {
	int copy_len = ndnfs::seg_size;
	if (copy_len > size_left)
	  copy_len = size_left;

	write_segment (path, ver, seg++, buf_pos, copy_len, fi);
	buf_pos += copy_len;
	size_left -= copy_len;
  }
  
out:
  int total_seg = seg > old_total_seg ? seg : old_total_seg;
  
  int ver_size = (int) (offset + size);
  // The new total size should be the maximum of old size and (size + offset)
  ver_size = ver_size > old_ver_size ? ver_size : old_ver_size;
  
  // Update temp version entry
  sqlite3_finalize (stmt);
  sqlite3_prepare_v2 (db, "UPDATE file_versions SET size = ?, totalSegments = ? WHERE path = ? AND version = ?;", -1, &stmt, 0);
  sqlite3_bind_int (stmt, 1, ver_size);
  sqlite3_bind_int (stmt, 2, total_seg);
  sqlite3_bind_text (stmt, 3, path, -1, SQLITE_STATIC);
  sqlite3_bind_int (stmt, 4, ver);
  int res = sqlite3_step (stmt);
  sqlite3_finalize (stmt);
  if (res != SQLITE_OK && res != SQLITE_DONE)
    return -1;
  
  return ver_size;
}


int truncate_version(const char* path, const int ver, off_t length)
{
#ifdef NDNFS_DEBUG
  cout << "truncate_version: path=" << path << std::dec << ", ver=" << ver << ", length=" << length << endl;
#endif

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
#ifdef NDNFS_DEBUG
  cout << "remove_version: path=" << path << ", ver=" << std::dec << ver << endl;
#endif

  remove_segments(path, ver);
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2(db, "DELETE FROM file_versions WHERE path = ? and version = ?;", -1, &stmt, 0);
  sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
  sqlite3_bind_int(stmt, 2, ver);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
}


void remove_versions(const char* path)
{
#ifdef NDNFS_DEBUG
  cout << "remove_versions: path=" << path << endl;
#endif

  sqlite3_stmt *stmt;
  sqlite3_prepare_v2(db, "SELECT current_version, temp_version FROM file_system WHERE path = ?;", -1, &stmt, 0);
  sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
  int res = sqlite3_step(stmt);
  if(res == SQLITE_ROW){
	int curr_ver = sqlite3_column_int(stmt, 0);
	if (curr_ver != -1) {
	  remove_version(path, curr_ver);
	}

	int tmp_ver = sqlite3_column_int(stmt, 1);
	if (tmp_ver != -1) {
	  remove_version(path, tmp_ver);
	}
  }
  sqlite3_finalize(stmt);
}
