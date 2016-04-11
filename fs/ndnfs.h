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

#ifndef NDNFS_H
#define NDNFS_H


#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <assert.h>
#include <iostream>
#include <sstream>
#include <string>
#include <dirent.h>

#include <time.h>
#include <sys/time.h>

#include <utime.h>

#include <pthread.h>

#define FUSE_USE_VERSION 26
#define HAVE_SETXATTR true
 
#include <fuse.h>
#ifdef HAVE_SETXATTR
#include <sys/types.h>
#include <sys/xattr.h>
#endif
#include <sqlite3.h>


#include <ndn-cpp/security/key-chain.hpp>
#include <ndn-cpp/security/identity/memory-identity-storage.hpp>
#include <ndn-cpp/security/identity/memory-private-key-storage.hpp>
#include <ndn-cpp/security/policy/no-verify-policy-manager.hpp>
#include <ndn-cpp/name.hpp>

#include <json/reader.h>
#include <json/value.h>
#include <json/writer.h>

#include "config.h"
#include "logger.h"

extern const char *db_name;
extern sqlite3 *db;
static Json::Value sqlite_returned_row;

//Convert metadataNames to C strings.
static const char* pathName = "user.path";
static const char* verName = "user.ver";
static const char* current_versionName = "user.current_version";
static const char* mime_typeName = "user.mime_type";
static const char* ready_signedName = "user.ready_signed";
static const char* fileTypeName = "user.type";
static const char* certificateNName = "user.certificate";


namespace ndnfs {
    extern ndn::Name certificateName;
    extern ndn::ptr_lib::shared_ptr<ndn::KeyChain> keyChain;
    extern std::string global_prefix;
    extern std::string root_path;
    extern std::string logging_path;
    
    

    extern const int version_type;
    extern const int segment_type;
    extern const int seg_size;
    extern const int seg_size_shift;

    extern int user_id;
    extern int group_id;
}

inline int split_last_component(const std::string &path, std::string &prefix, std::string &name)
{
    size_t last_comp_pos = path.rfind('/');
    if (last_comp_pos == std::string::npos)
        return -1;
    
    prefix = path.substr(0, last_comp_pos);
    if (prefix.empty())
        prefix = "/";
    name = path.substr(last_comp_pos + 1);
    
    return 0;
}

void abs_path(char *dest, const char *path);

#endif
