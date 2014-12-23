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

#include <iostream>

#include <ndn-cpp/face.hpp>
#include <ndn-cpp/name.hpp>
#include <ndn-cpp/common.hpp>
#include <ndn-cpp/security/key-chain.hpp>

#include <sqlite3.h>
#include <unistd.h>

#include "servermodule.h"

using namespace std;
using namespace ndn;

const char* db_name = "/tmp/ndnfs.db";
const char* fs_path = "/tmp/ndnfs";
const char* fs_prefix = "/ndn/broadcast/ndnfs";

sqlite3 *db;

Face face;
KeyChain keyChain;
Name certificateName;

string global_prefix;

void usage() {
    fprintf(stderr, "Usage: ./ndnfs-server [-p serving prefix][-d db file][-f file system root]\n");
    exit(1);
}

int main(int argc, char **argv) {
    certificateName = keyChain.getDefaultCertificateName();
    face.setCommandSigningInfo(keyChain, certificateName);
    
    int opt;
    
    while ((opt = getopt(argc, argv, "p:d:")) != -1) {
        switch (opt) {
        case 'p':
            fs_prefix = optarg;
            break;
        case 'd':
            db_name = optarg;
            break;
        case 'f':
            fs_path = optarg;
            break;
        default:
            usage();
            break;
        }
    }
        
    cout << "main: open sqlite database" << endl;

    if (sqlite3_open(db_name, &db) == SQLITE_OK) {
        cout << "main: ok" << endl;
    } else {
        cout << "main: cannot connect to sqlite db, quit" << endl;
        sqlite3_close(db);
        return -1;
    }

    cout << "serving prefix: " << fs_prefix << endl;
    cout << "db file: " << db_name << endl;
    cout << "fs root path: " << fs_path << endl;
    
    Name prefixName(fs_prefix);
    global_prefix = prefixName.toUri();

    face.registerPrefix(prefixName, ::onInterest, ::onRegisterFailed);
    while (true) {
        face.processEvents();
        usleep (10000);
    }

    cout << "main(): ServerModule exiting ..." << endl;

    return 0;
}

