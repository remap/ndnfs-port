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

const char *db_name = "/tmp/ndnfs.db";
sqlite3 *db;

Face face("localhost");
KeyChain keyChain;
Name certificateName;

string global_prefix;

int main(int argc, char **argv) {
    certificateName = keyChain.getDefaultCertificateName();
    face.setCommandSigningInfo(keyChain, certificateName);
    const char* prefix = "/ndn/edu/ucla/remap/ndnfs";

    int opt;
    while ((opt = getopt(argc, argv, "p:d:")) != -1) {
        switch (opt) {
        case 'p':
            prefix = optarg;
            break;
        case 'd':
            db_name = optarg;
            break;
        default:
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

    cout << "serving prefix: " << prefix << endl;
    cout << "db file: " << db_name << endl;
    
    Name prefixName(prefix);
    global_prefix = prefixName.toUri();

    face.registerPrefix(prefixName, ::onInterest, ::onRegisterFailed);
    while (true) {
        face.processEvents();
        usleep (10);
    }

    cout << "main(): ServerModule exiting ..." << endl;

    return 0;
}

