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
#include <fcntl.h>

#include "servermodule.h"
#include <ndn-cpp/face.hpp>
#include <ndn-cpp/interest.hpp>
#include <ndn-cpp/security/key-chain.hpp>
#include <ndn-cpp/common.hpp>

#include <ndn-cpp/sha256-with-rsa-signature.hpp>

using namespace std;
using namespace ndn;

// should put in a shared config with ndnfs implementation
const int BLOCKSIZE = 8192;
const int SEGSIZESHIFT = 13;

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

int parseName(const ndn::Name& name, int &version, int &seg, string &path) {
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

    path = path.substr(global_prefix.length());
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

void processInterest(const Name& interest_name, Transport& transport) {
    string path;
    int version;
    int seg;
    int ret = parseName(interest_name, version, seg, path);
#ifdef NDNFS_DEBUG
    cout << "processName(): version=" << version << ", segment=" << seg << ", path=" << path << ", ret value=" << ret << endl;
#endif
    // The client is asking for a segment of a file.
    if (ret == 3) {
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
        // instead of reading data from the sqlite database, we find the corresponding 
        // segment from the file system, and assemble it with signature taken from the 
        // sqlite database
        const char * signatureBlob = (const char *)sqlite3_column_blob(stmt, 3);
        int len = sqlite3_column_bytes(stmt, 3);
        sqlite3_finalize(stmt);
#ifdef NDNFS_DEBUG
        cout << "processName(): blob signature length=" << len << endl;
        cout << "processName(): blob signature is " << endl;
        
        for (int i = 0; i < len; i++) {
            printf("%02x", (unsigned char)signatureBlob[i]);
        }
        cout << endl;
#endif
        // For now, the signature type is assumed to be Sha256withRSA; should read
        // signature type from database, which is not yet implemented in the database
        Sha256WithRsaSignature signature;
        
        signature.setSignature(Blob((const uint8_t *)signatureBlob, len));
        
        Data data;
        data.setName(interest_name);
        data.setSignature(signature);

        // When assembling the data packet, finalblockid should be put into each segment,
        // this means when reading each segment, file_version also needs to be consulted for the finalBlockId.
        
        sqlite3_stmt *stmt2;
        sqlite3_prepare_v2(db, "SELECT * FROM file_versions WHERE path = ? AND version = ? ", -1, &stmt2, 0);
        sqlite3_bind_text(stmt2, 1, path.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt2, 2, version);
        if(sqlite3_step(stmt2) != SQLITE_ROW){
            sqlite3_finalize(stmt2);
            return;
        }
		
		int totalSegmentNumber = sqlite3_column_int(stmt2,3);
        sqlite3_finalize(stmt2);
        
        if (totalSegmentNumber > 0) {
          // in the JS plugin, finalBlockId component is parsed with toSegment
          // not sure if it's supposed to be like this in other ndn applications,
          // if so, should consider adding wrapper in library 'toSegment' (returns Component), instead of just 'appendSegment' (returns Name)
		  Name::Component finalBlockId = Name::Component::fromNumberWithMarker(totalSegmentNumber - 1, 0x00);
		  data.getMetaInfo().setFinalBlockId(finalBlockId);
        }
        
        int fd;
		// Opening in server module has clue that the path is relative to fs_path
		char file_path[FULL_LEN] = "";
		strcpy(file_path, fs_path);
		strcat(file_path, path.c_str());
		fd = open(file_path, O_RDONLY);
	    
	    if (fd == -1) {
	      cout << "Open " << file_path << " failed." << endl;
	      return;
	    }
	    
		char output[BLOCKSIZE] = "";
		int actualLen = pread(fd, output, BLOCKSIZE, seg << SEGSIZESHIFT);
	    
		close(fd);
        
	    if (actualLen == -1) {
	      cout << "Read from " << file_path << " failed." << endl;
	      return;
	    }
	    
        data.setContent((uint8_t*)output, actualLen);
        
        Blob encodedData = data.wireEncode();
        transport.send(*encodedData);
#ifdef NDNFS_DEBUG
        cout << "processName(): content object returned and interest consumed" << endl;
#endif
    }
    // The client is asking for a certain version of a file
    else if (ret == 2) {
        sqlite3_stmt *stmt;
        sqlite3_prepare_v2(db, "SELECT * FROM file_versions WHERE path = ? AND version = ? ", -1, &stmt, 0);
        sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, version);
        if (sqlite3_step(stmt) != SQLITE_ROW){
#ifdef NDNFS_DEBUG
            cout << "processName(): no such file/directory found in ndnfs: " << path << endl;
#endif
            sqlite3_finalize(stmt);
            return;
        }
		
        sendFile(path, version, sqlite3_column_int(stmt,2), sqlite3_column_int(stmt,3), transport);
        sqlite3_finalize(stmt);
    }
    // The client is asking for 'generic' info about a file/folder in ndnfs
    else if (ret == 1) {
        sqlite3_stmt *stmt;
        sqlite3_prepare_v2(db, "SELECT * FROM file_system WHERE path = ?", -1, &stmt, 0);
        sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) != SQLITE_ROW) {
#ifdef NDNFS_DEBUG
            cout << "processName(): no such file/directory found in ndnfs: " << path << endl;
#endif
            sqlite3_finalize(stmt);
            return;
        }
        
        int type = sqlite3_column_int(stmt,2);
        if (type == 1) {
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
    // The client is asking for the meta_info of a file
    else if (ret == 4) {
        sqlite3_stmt *stmt;
        sqlite3_prepare_v2(db, "SELECT * FROM file_system WHERE path = ?", -1, &stmt, 0);
        sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_STATIC);
        if(sqlite3_step(stmt) != SQLITE_ROW) {
#ifdef NDNFS_DEBUG
            cout << "processName(): no such file/directory found in ndnfs: " << path << endl;
#endif
            sqlite3_finalize(stmt);
            return;
        }
        
		// right now, if the requested path is a file, whenever meta_info is asked, the server replies with 
		// name: <received name>/<mime_type>, content: mime_type	
		// if the requested path is a folder, the server does not reply with anything	
        string mimeType = string((char *)sqlite3_column_text(stmt, 9));
        cout << mimeType << endl;
        
        int type = sqlite3_column_int(stmt,2);
        if (type == 1) {
#ifdef NDNFS_DEBUG
            cout << "processName(): found file: " << path << endl;
#endif
            version = sqlite3_column_int(stmt, 7);
            sqlite3_finalize(stmt);
            sqlite3_prepare_v2(db, "SELECT * FROM file_versions WHERE path = ? AND version = ? ", -1, &stmt, 0);
            sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 2, version);
            
            if (sqlite3_step(stmt) != SQLITE_ROW) {
#ifdef NDNFS_DEBUG
                cout << "processName(): no such file version found in ndnfs: " << path << endl;
#endif
                sqlite3_finalize(stmt);
                return;
            }
            
            Name metaName(interest_name);
            metaName.append(NdnfsNamespace::mimeComponentName_);
            
            Data data(metaName);
            data.setContent((const uint8_t *)&mimeType[0], mimeType.size());
            keyChain->sign(data, certificateName);
            transport.send(*data.wireEncode());
            
            sqlite3_finalize(stmt);
        } else {
#ifdef NDNFS_DEBUG
            cout << "processName(): found dir: " << path << endl;
            cout << "processName(): meta_info of a dir is not defined yet." << endl;
#endif
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
    name.append(Name(path));
    
    Blob ndnfsFileComponent = Name::fromEscapedString(NdnfsNamespace::fileComponentName_);
    name.append(ndnfsFileComponent).appendVersion(version);
    Data data0;
    data0.setName(name);
    
    Name::Component finalBlockId = Name::Component::fromNumberWithMarker(totalseg - 1, 0x00);
    
    data0.getMetaInfo().setFinalBlockId(finalBlockId);
    data0.setContent((uint8_t*)wireData, size);
    
    keyChain->sign(data0, certificateName);
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
        
        Name name(global_prefix);
        name.append(Name(path));
        
        Blob ndnfsDirComponent = Name::fromEscapedString(NdnfsNamespace::dirComponentName_);
        name.append(ndnfsDirComponent).appendVersion(mtime);
        Data data0;
        data0.setName(name);
        data0.setContent((uint8_t*)wireData, size);
        
        keyChain->sign(data0, certificateName);
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
