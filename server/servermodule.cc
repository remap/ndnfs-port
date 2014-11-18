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
#include <string>
#include <vector>

#include "servermodule.h"
#include <ndn-cpp/face.hpp>
#include <ndn-cpp/interest.hpp>
#include <ndn-cpp/security/key-chain.hpp>
#include <ndn-cpp/common.hpp>

using namespace std;
using namespace ndn;

void onInterest(const ptr_lib::shared_ptr<const Name>& prefix, const ptr_lib::shared_ptr<const Interest>& interest, Transport& transport, uint64_t registeredPrefixId) {
#ifdef NDNFS_DEBUG
    cout << "------------------------------------------------------------" << endl;
    cout << "OnInterest(): interest name: " << interest->getName() << endl;
#endif
    processInterest(interest->getName(), transport);
#ifdef NDNFS_DEBUG
    cout << "OnInterest(): Done" << endl;
    cout << "------------------------------------------------------------" << endl;
#endif
}

void onRegisterFailed(const ptr_lib::shared_ptr<const Name>& prefix) {
    cout << "Register failed" << endl;
}

void parseName(const ndn::Name& name, int &version, int &seg, string &path) {
    version = -1;
    seg = -1;
    ostringstream oss;
    ndn::Name::const_iterator iter = name.begin();
    for (; iter != name.end(); iter++) {
#ifdef NDNFS_DEBUG
        cout << "ndnName2String(): interest name component: " << iter->toEscapedString() << endl;
#endif
        const uint8_t marker = *(iter->getValue().buf());
        //cout << (unsigned int)marker << endl;
        if (marker == 0xFD) {
            version = iter->toVersion(); 
        }
        else if (marker == 0x00) {
            seg = iter->toSegment();
        }
        else if (marker == 0xC1) {
            continue;
        }
        else {
            string comp = iter->toEscapedString();
            oss << "/" << comp;
        }
    }
    path = oss.str();
#ifdef NDNFS_DEBUG
    cout << "ndnName2String(): full path: " << path << endl;
#endif
    path = path.substr(global_prefix.length());
    if (path == "")
        path = string("/");
#ifdef NDNFS_DEBUG
    cout << "ndnName2String(): file path after removing global prefix: " << path << endl;
#endif
}

void processInterest(const Name& interest_name, Transport& transport) {
    string path;
    int version;
    int seg;
    parseName(interest_name, version, seg, path);
#ifdef NDNFS_DEBUG
    cout << "processName(): version=" << version << ", segment=" << seg << ", path=" << path << endl;
#endif
    if(version != -1 && seg != -1){
        sqlite3_stmt *stmt;
        sqlite3_prepare_v2(db, "SELECT * FROM file_segments WHERE path = ? AND version = ? AND segment = ?", -1, &stmt, 0);
        sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, version);
        sqlite3_bind_int(stmt, 3, seg);
        if(sqlite3_step(stmt) != SQLITE_ROW){
#ifdef NDNFS_DEBUG
            cout << "processName(): no such file/directory found in ndnfs: " << path << endl;
#endif
            sqlite3_finalize(stmt);
            return;
        }
#ifdef NDNFS_DEBUG
        cout << "processName(): a match has been found for prefix: " << interest_name << endl;
        cout << "processName(): fetching content object from database" << endl;
#endif       
        const char * data = (const char *)sqlite3_column_blob(stmt, 3);
        int len = sqlite3_column_bytes(stmt, 3);
#ifdef NDNFS_DEBUG
        cout << "processName(): blob length=" << len << endl;
        cout << "processName(): blob data is " << endl;
        //ofstream ofs("/tmp/blob", ios_base::binary);
        for (int i = 0; i < len; i++) {
            printf("%02x", (unsigned char)data[i]);
            //ofs << data[i];
        }
        cout << endl;
#endif
        transport.send((uint8_t*)data, len);
#ifdef NDNFS_DEBUG
        cout << "processName(): content object returned and interest consumed" << endl;
#endif
        sqlite3_finalize(stmt);
    }
    else if (version != -1 && seg == -1) {
        sqlite3_stmt *stmt;
        sqlite3_prepare_v2(db, "SELECT * FROM file_versions WHERE path = ? AND version = ? ", -1, &stmt, 0);
        sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, version);
        if(sqlite3_step(stmt) != SQLITE_ROW){
#ifdef NDNFS_DEBUG
            cout << "processName(): no such file/directory found in ndnfs: " << path << endl;
#endif
            sqlite3_finalize(stmt);
            return;
        }
		
        sendFile(path, version, sqlite3_column_int(stmt,2), sqlite3_column_int(stmt,3), transport);
        sqlite3_finalize(stmt);
    }
    else if (version == -1 && seg == -1) {
        sqlite3_stmt *stmt;
        sqlite3_prepare_v2(db, "SELECT * FROM file_system WHERE path = ?", -1, &stmt, 0);
        sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_STATIC);
        if(sqlite3_step(stmt) != SQLITE_ROW){
#ifdef NDNFS_DEBUG
            cout << "processName(): no such file/directory found in ndnfs: " << path << endl;
#endif
            sqlite3_finalize(stmt);
            return;
        }
        
        int type = sqlite3_column_int(stmt,2);
        if(type == 1){
#ifdef NDNFS_DEBUG
            cout << "processName(): found file: " << path << endl;
#endif
            version = sqlite3_column_int(stmt, 7);
            sqlite3_finalize(stmt);
            sqlite3_prepare_v2(db, "SELECT * FROM file_versions WHERE path = ? AND version = ? ", -1, &stmt, 0);
            sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 2, version);
            if(sqlite3_step(stmt) != SQLITE_ROW){
#ifdef NDNFS_DEBUG
                cout << "processName(): no such file version found in ndnfs: " << path << endl;
#endif
                sqlite3_finalize(stmt);
                return;
            }
            
            sendFile(path, version, sqlite3_column_int(stmt,2), sqlite3_column_int(stmt,3), transport);
            sqlite3_finalize(stmt);
        } else {
#ifdef NDNFS_DEBUG
            cout << "processName(): found dir: " << path << endl;
#endif
            int mtime = sqlite3_column_int(stmt, 5);
            sqlite3_finalize(stmt);
            sendDir(path, mtime, transport);
        }
    }
}

void sendFile(const string& path, int version, int sizef, int totalseg, Transport& transport) {
    ndnfs::FileInfo infof;
    infof.set_size(sizef);
    infof.set_totalseg(totalseg);
    infof.set_version(version);
    int size = infof.ByteSize();
    
    char *wireData = new char[size];
    infof.SerializeToArray(wireData, size);
    Name name(global_prefix);
    
    // sendDir is using string concatenation instead
    Name fileName(path);
    for (int i = 0; i < fileName.size(); i++) {
        name.append(fileName.get(i));
    }
    
    Blob ndnfsFileComponent = Name::fromEscapedString("%C1.FS.file");
    name.append(ndnfsFileComponent).appendVersion(version);
    Data data0;
    data0.setName(name);
    data0.setContent((uint8_t*)wireData, size);
    
    keyChain.sign(data0, certificateName);
    transport.send(*data0.wireEncode());
    
    cout << "Data returned with name: " << name.toUri() << endl;
    
    delete wireData;
    return;
}

void sendDir(const string& path, int mtime, Transport& transport) {
    //finding the relevant file recursively
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "SELECT * FROM file_system WHERE parent = ?", -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_STATIC);
    
    ndnfs::DirInfoArray infoa;
    int count = 0;
    while(sqlite3_step(stmt) == SQLITE_ROW){
        ndnfs::DirInfo *info = infoa.add_di();
        info->set_type(sqlite3_column_int(stmt, 2));
        info->set_path((const char *)sqlite3_column_text(stmt, 0));
        count++;
    }
    sqlite3_finalize(stmt);
    
    if (count != 0) {
        int size = infoa.ByteSize();
        char *wireData = new char[size];
        infoa.SerializeToArray(wireData, size);
        
        // string concatenation works here; 
        Name name(global_prefix + path);
        
        Blob ndnfsDirComponent = Name::fromEscapedString("%C1.FS.dir");
        name.append(ndnfsDirComponent).appendVersion(mtime);
        Data data0;
        data0.setName(name);
        data0.setContent((uint8_t*)wireData, size);
        
        keyChain.sign(data0, certificateName);
        transport.send(*data0.wireEncode());
        delete wireData;
    }
    else {
#ifdef NDNFS_DEBUG
        cout << "MatchFile(): no such file found in path: " << path << endl;
#endif
    }
    return;
}
