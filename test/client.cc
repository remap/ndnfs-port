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
 * Author: Qiuhan Ding <dingqiuhan@gmail.com>
 *         Zhehao Wang <wangzhehao410305@gmail.com>
 */

#include <ndn-cpp/common.hpp>
#include <ndn-cpp/data.hpp>
#include <ndn-cpp/interest.hpp>
#include <ndn-cpp/face.hpp>

#include "console-handling.h"
#include "handler.h"

#include <iostream>
#include <getopt.h>
#include <unistd.h>
#include <string.h>

using namespace std;
using namespace ndn;

#define FETCH_COMMAND "fetch "
#define SHOW_COMMAND "show "
#define HELP_COMMAND "help "

Face face("localhost");

bool done = false;

void usage() {
    fprintf(stderr, "Client is a command line browser for ndnfs. \nUsage: ./client\n");
    exit(1);
}

int main (int argc, char **argv) {
    int opt;
    while ((opt = getopt(argc, argv, "h:")) != -1) {
        switch (opt) {
        case 'h': 
            usage();
            break;
        default: 
            usage();
            break;
        }
    }

	while (true) {
        if (isStdinReady()) {
            string input = stdinReadLine();
            if (strstr(input.c_str(), SHOW_COMMAND)) {
                string nameStr = input.substr(strlen(SHOW_COMMAND));
                cout << "Displaying info of file by name: " << nameStr << endl;
                
                Handler handler;
                Name name(nameStr);
                Interest interest(name);
                face.expressInterest
                  (interest, ptr_lib::bind(&Handler::onData, &handler, _1, _2), 
                   ptr_lib::bind(&Handler::onTimeout, &handler, _1));
            }
            else if (strstr(input.c_str(), FETCH_COMMAND)) {
                string nameStr = input.substr(strlen(FETCH_COMMAND));
                
            }
            else if (strstr(input.c_str(), HELP_COMMAND)) {
            
            }
            else {
                cout << input << ": Command unknown." << endl;
            }
        }

		face.processEvents();
		// We need to sleep for a few milliseconds so we don't use 100% of the CPU.
		usleep(10000);
    }
    
    return 0;
}
