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
 * Author: Qiuhan Ding <dingqiuhan@gmail.com>
 *         Wentao Shang <wentao@cs.ucla.edu>
 *         Zhehao Wang <wangzhehao410305@gmail.com>
 */

#include <iostream>

#include "server.h"
#include "servermodule.h"

using namespace std;

string ndnfs::server::db_name = "/tmp/ndnfs.db";
string ndnfs::server::fs_path = "/tmp/ndnfs";
string ndnfs::server::fs_prefix = "/ndn/broadcast/ndnfs";

const int ndnfs::server::dir_type = 0;
const int ndnfs::server::file_type = 1;

const int ndnfs::server::seg_size = 8192;
const int ndnfs::server::seg_size_shift = 13;

sqlite3 *ndnfs::server::db;
ndn::ptr_lib::shared_ptr<ndn::KeyChain> ndnfs::server::keyChain;
ndn::Name ndnfs::server::certificateName;

ndn::Face face;

void abs_path(char *dest, const char *src)
{
  strcpy(dest, ndnfs::server::fs_path.c_str());
  strcat(dest, src);
}

void usage() {
  fprintf(stderr, "Usage: ./ndnfs-server [-p serving prefix][-d db file][-f file system root]\n");
  exit(1);
}

int main(int argc, char **argv) {
  // log configuration
  Log<Output2FILE>::reportingLevel() = LOG_DEBUG;
  FILE* log_fd = fopen( "ndnfs-server.log", "w" );
  Output2FILE::stream() = log_fd;
  
  FILE_LOG(LOG_DEBUG) << "NDNFS logging";

  // Initialize the keychain
  ndn::ptr_lib::shared_ptr<ndn::MemoryIdentityStorage> identityStorage(new ndn::MemoryIdentityStorage());
  ndn::ptr_lib::shared_ptr<ndn::MemoryPrivateKeyStorage> privateKeyStorage(new ndn::MemoryPrivateKeyStorage());
  ndnfs::server::keyChain.reset
	(new ndn::KeyChain
	  (ndn::ptr_lib::make_shared<ndn::IdentityManager>
		(identityStorage, privateKeyStorage), ndn::ptr_lib::shared_ptr<ndn::NoVerifyPolicyManager>
		  (new ndn::NoVerifyPolicyManager())));
  
  // Initialize the storage.
  ndn::Name keyName("/testname/DSK-123");
  ndnfs::server::certificateName = keyName.getSubName(0, keyName.size() - 1).append("KEY").append
		 (keyName.get(keyName.size() - 1)).append("ID-CERT").append("0");
  identityStorage->addKey(keyName, ndn::KEY_TYPE_RSA, ndn::Blob(DEFAULT_RSA_PUBLIC_KEY_DER, sizeof(DEFAULT_RSA_PUBLIC_KEY_DER)));
  privateKeyStorage->setKeyPairForKeyName
	(keyName, ndn::KEY_TYPE_RSA, DEFAULT_RSA_PUBLIC_KEY_DER,
	 sizeof(DEFAULT_RSA_PUBLIC_KEY_DER), DEFAULT_RSA_PRIVATE_KEY_DER,
	 sizeof(DEFAULT_RSA_PRIVATE_KEY_DER));
  
  face.setCommandSigningInfo(*ndnfs::server::keyChain, ndnfs::server::certificateName);
  
  int opt;
  while ((opt = getopt(argc, argv, "p:d:f:")) != -1) {
	switch (opt) {
	case 'p':
	  ndnfs::server::fs_prefix.assign(optarg);
	  break;
	case 'd':
	  ndnfs::server::db_name.assign(optarg);
	  break;
	case 'f':
	  ndnfs::server::fs_path.assign(optarg);
	  break;
	default:
	  usage();
	  break;
	}
  }
	  
  cout << "main: open sqlite database" << endl;

  if (sqlite3_open(ndnfs::server::db_name.c_str(), &ndnfs::server::db) == SQLITE_OK) {
	cout << "main: ok" << endl;
  } else {
	cout << "main: cannot connect to sqlite db, quit" << endl;
	sqlite3_close(ndnfs::server::db);
	return -1;
  }

  cout << "serving prefix: " << ndnfs::server::fs_prefix << endl;
  cout << "db file: " << ndnfs::server::db_name << endl;
  cout << "fs root path: " << ndnfs::server::fs_path << endl;
  
  ndn::Name prefix_name(ndnfs::server::fs_prefix);
  
  face.registerPrefix(prefix_name, ::onInterest, ::onRegisterFailed);
  while (true) {
	face.processEvents();
	usleep (10000);
  }

  cout << "main(): ServerModule exiting ..." << endl;

  return 0;
}

