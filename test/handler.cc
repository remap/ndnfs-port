// handler class for each ndnfs client request
#include "handler.h"

// protobuf defining file/folder fields
#include "dir.pb.h"
#include "file.pb.h"

#include <iostream>

using namespace ndn;
using namespace std;

Handler::Handler() {
    done_ = false;
}

Handler::~Handler() {

}

void Handler::onData(const ptr_lib::shared_ptr<const Interest>& interest, const ptr_lib::shared_ptr<Data>& data) {
    const Blob& content = data->getContent();
    const Name& data_name = data->getName();
    const Name::Component& comp = data_name.get(data_name.size() - 2);
    string marker = comp.toEscapedString();
    if(marker == "%C1.FS.dir"){
        ndnfs::DirInfoArray infoa;
        if(infoa.ParseFromArray(content.buf(),content.size()) && infoa.IsInitialized()){
            cout << "This is a directory:" << endl;
            int n = infoa.di_size();
            for(int i = 0; i<n; i++){
                const ndnfs::DirInfo &info = infoa.di(i);
                cout << info.path();
                if(info.type() == 0)
                    cout <<":    DIR"<<endl;
                else
                    cout <<":    FILE"<<endl;
            }
        }
        else{
            cerr << "protobuf error" << endl;
        }
    }
    else if(marker == "%C1.FS.file"){
        ndnfs::FileInfo infof;
        if(infof.ParseFromArray(content.buf(),content.size()) && infof.IsInitialized()){
            cout << "This is a file" << endl;
            cout << "name:  " << data->getName().toUri() << endl;
            cout << "size:  " << infof.size() << endl;
            cout << "version:   " << infof.version() << endl;
            cout << "total segments: " << infof.totalseg() << endl;
        }
        else{
            cerr << "protobuf error" << endl;
        }
    }
    else {
        cout << "data: " << string((char*)content.buf(), content.size()) << endl;
        cout << "fbi: " << data->getMetaInfo().getFinalBlockId().toSegment() << endl;
    }

    done_ = true;
}

void Handler::onTimeout(const ptr_lib::shared_ptr<const Interest>& interest) {
    cout << "Timeout " << interest->getName().toUri() << endl;
    done_ = true;
}
