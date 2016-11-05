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
#include <boost/asio.hpp>
#include <ndn-cpp/threadsafe-face.hpp>

#include "server.h"
#include "servermodule.h"

using namespace std;

string ndnfs::server::db_name = "/tmp/ndnfs.db";
string ndnfs::server::fs_path = "/tmp/ndnfs";
string ndnfs::server::fs_prefix = "/ndn/broadcast/ndnfs";
string ndnfs::server::logging_path = "";

const int ndnfs::server::seg_size = 8192;
const int ndnfs::server::seg_size_shift = 13;
const int ndnfs::server::default_freshness_period = 5000;

sqlite3 *ndnfs::server::db;
ndn::ptr_lib::shared_ptr<ndn::KeyChain> ndnfs::server::keyChain;
ndn::Name ndnfs::server::certificateName;

boost::asio::io_service ioService;
ndn::ThreadsafeFace face(ioService);

void abs_path(char *dest, const char *src)
{
  strcpy(dest, ndnfs::server::fs_path.c_str());
  strcat(dest, src);
}

void usage() {
  fprintf(stderr, "Usage: ./ndnfs-server [-p serving prefix][-f file system root][-l logging file path][-d db file]\n");
  exit(1);
}

int main(int argc, char **argv) {
  // Parse command parameters
  int opt;
  while ((opt = getopt(argc, argv, "p:f:l:d:")) != -1) {
	switch (opt) {
	case 'p':
	  ndnfs::server::fs_prefix.assign(optarg);
	  break;
	case 'f':
	  ndnfs::server::fs_path.assign(optarg);
	  break;
	case 'l':
	  ndnfs::server::logging_path.assign(optarg);
	  break;
	case 'd':
	  ndnfs::server::db_name.assign(optarg);
	  break;
	default:
	  usage();
	  break;
	}
  }
  
  // TODO: debug daemonize start failure on OSX
  /*
  pid_t pid, sid;
  pid = fork();
  if (pid < 0) {
    cerr << "main: fork PID < 0" << endl;
	exit(EXIT_FAILURE);
  }
  if (pid > 0) {
    cerr << "main: daemonize start" << endl;
	exit(EXIT_SUCCESS);
  }

  umask(0);
  */

  // Set up logging
  Log<Output2FILE>::reportingLevel() = LOG_DEBUG;
  FILE* log_fd = fopen(ndnfs::server::logging_path.c_str(), "w" );
  if (ndnfs::server::logging_path == "" || log_fd == NULL) {
	Output2FILE::stream() = stdout;
  } else {
    Output2FILE::stream() = log_fd;
  }
  
  FILE_LOG(LOG_DEBUG) << "Ndnfs-server logging." << endl;
  
  /*
  sid = setsid();
  if (sid < 0) {
    cerr << "main: setsid sid < 0" << endl;
	exit(EXIT_FAILURE);
  }

  if ((chdir("/")) < 0) {
    cerr << "main: chdir failed." << endl;
	exit(EXIT_FAILURE);
  }

  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);
  */

  // Actual ndnfs code
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
  
  if (sqlite3_open(ndnfs::server::db_name.c_str(), &ndnfs::server::db) == SQLITE_OK) {
    FILE_LOG(LOG_DEBUG) << "main: sqlite database open ok" << endl;
  } else {
	FILE_LOG(LOG_DEBUG) << "main: cannot connect to sqlite db: " << ndnfs::server::db_name << ", quit" << endl;
	sqlite3_close(ndnfs::server::db);
	return -1;
  }

  FILE_LOG(LOG_DEBUG) << "main: db file: " << ndnfs::server::db_name << endl;
  FILE_LOG(LOG_DEBUG) << "main: fs root path: " << ndnfs::server::fs_path << endl;
  
  ndn::Name prefix_name(ndnfs::server::fs_prefix);
  
  face.registerPrefix(prefix_name, (const ndn::OnInterestCallback&)::onInterestCallback, ::onRegisterFailed);
  
  FILE_LOG(LOG_DEBUG) << "main: serving prefix: " << ndnfs::server::fs_prefix << endl;
  
  // Use work to keep ioService running.
  boost::asio::io_service::work work(ioService);
  ioService.run();

  FILE_LOG(LOG_DEBUG) << "main: server exit." << endl;
  
  return 0;
}

