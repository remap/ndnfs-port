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

#ifndef __SERVER_MODULE_H__
#define __SERVER_MODULE_H__

#define NDNFS_DEBUG

#include <string>

#include <ndn-cpp/face.hpp>
#include <ndn-cpp/common.hpp>
#include <ndn-cpp/name.hpp>
#include <ndn-cpp/interest.hpp>
#include <ndn-cpp/security/key-chain.hpp>
#include <ndn-cpp/security/identity/osx-private-key-storage.hpp>
#include <ndn-cpp/security/identity/memory-private-key-storage.hpp>

#include "dir.pb.h"
#include "file.pb.h"
#include "namespace.h"
#include "server.h"

#include <sqlite3.h>

void onInterest(const ndn::ptr_lib::shared_ptr<const ndn::Name>& prefix, const ndn::ptr_lib::shared_ptr<const ndn::Interest>& interest, ndn::Transport& transport, uint64_t registeredPrefixId);

void onRegisterFailed(const ndn::ptr_lib::shared_ptr<const ndn::Name>& prefix);

/**
 * For all interest received by ndnfs-server, it parses the name for different actions.
 * Parse returns an integer, signifying the type of request.
 *
 * Proposed patterns:
 * <root>/<path>: 1, check if <path> exists either as a file, or a directory;
 *   return name: <root>/<path>/C1.FS.FILE/<version>, content: file.proto encoded; if file
 *   return name: <root>/<path>/C1.FS.DIR/<version>, content: dir.proto encoded; if folder
 * <root>/<path>/<version>: 2, check if <path>/<version> exists in db, should it work only with file, or file/folder both?
 * <root>/<path>/<version>/<segment>: 3, check if <path>/<version>/<segment> exists as a segment of a file
 *   return name: same, content: actual file content assembled with signature
 * <root>/<path>/<version>/_meta: 4, look for the meta of a certain version of a file
 * Otherwise return -1, we received a name that does not fit in any of these patterns.
 *
 * The difference with original implementation here would be, 
 * the original one extracts everything without caring about the sequence;
 * the sequence matters, because at some point in the new design, 
 * <root>/<path>/<version>/_meta/<segment> and <root>/<path>/<version>/<segment>/_meta
 * may both be valid. And wrong sequence in received name should not fetch back stuff.
 */
int parseName(const ndn::Name& name, int &version, int &seg, std::string &path);

void processInterest(const ndn::Name& interest_name, ndn::Transport& transport);

void sendDir(std::string path, ndn::Transport& transport);

void sendFile(const std::string& path, const std::string& mimeType, int version, ndn::Transport& transport);

void readFileSize(std::string path, int& file_size, int& total_seg);

#endif // __SERVER_MODULE_H__
