// handler class for each ndnfs client request
#include "handler.h"

// protobuf defining file/folder fields
#include "dir.pb.h"
#include "file.pb.h"

#include <iostream>
#include <fstream>

using namespace ndn;
using namespace std;

Handler::Handler(Face &face, KeyChain &keyChain, string nameStr, string fileName, bool fetchFile, bool doVerification) :
  face_(face), keyChain_(keyChain), nameStr_(nameStr), 
  fileName_(fileName), fetchFile_(fetchFile), doVerification_(doVerification),
  done_(false), currentSegment_(0), totalSegment_(0)
{
}

Handler::~Handler() {

}

void Handler::onAttrData(const ptr_lib::shared_ptr<const Interest>& interest, const ptr_lib::shared_ptr<Data>& data) {
    const Blob& content = data->getContent();
    const Name& data_name = data->getName();
    const Name::Component& comp = data_name.get(data_name.size() - 2);
    string marker = comp.toEscapedString();
    if (marker == "%C1.FS.dir") {
        ndnfs::DirInfoArray infoa;
        if (infoa.ParseFromArray(content.buf(),content.size()) && infoa.IsInitialized()) {
            cout << "This is a directory:" << endl;
            int n = infoa.di_size();
            for (int i = 0; i<n; i++) {
                const ndnfs::DirInfo &info = infoa.di(i);
                cout << info.path();
                if(info.type() == 0)
                    cout << ":    DIR" << endl;
                else
                    cout << ":    FILE" << endl;
            }
            if (fetchFile_) {
                cout << "Cannot fetch a directory." << endl;
            }
        }
        else{
            cerr << "protobuf error" << endl;
        }
    }
    else if (marker == "%C1.FS.file") {
        ndnfs::FileInfo infof;
        if(infof.ParseFromArray(content.buf(),content.size()) && infof.IsInitialized()){
            cout << "This is a file" << endl;
            cout << "name:  " << data->getName().toUri() << endl;
            cout << "size:  " << infof.size() << endl;
            cout << "version:   " << infof.version() << endl;
            cout << "total segments: " << infof.totalseg() << endl;
            
            totalSegment_ = infof.totalseg();
            
            if (fetchFile_) {
                Name fileName = data_name.getPrefix(data_name.size() - 2);
				fileName.appendVersion((uint64_t)infof.version()).appendSegment(0);
			
				Interest interest(fileName);

				face_.expressInterest
				  (interest, bind(&Handler::onFileData, this, _1, _2), 
				   bind(&Handler::onTimeout, this, _1));
            }
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

void Handler::onFileData (const ptr_lib::shared_ptr<const Interest>& interest, const ptr_lib::shared_ptr<Data>& data) {
    Name name = data->getName();
    cout << "FinalBlockId : " << data->getMetaInfo().getFinalBlockId().toSegment() << endl;
    
    if (doVerification_) {
		keyChain_.verifyData
		  (data, bind(&Handler::onVerified, this, _1), 
		   bind(&Handler::onVerifyFailed, this, _1));
		
		if (fileName_ != "") {
			ofstream writeFile;
			writeFile.open (fileName_);
			for (size_t i = 0; i < data->getContent().size(); ++i) {
      		    writeFile << (*data->getContent())[i];
      		}
			writeFile.close();
		}
	}
	else {
		cout << "Verification skipped." << endl;
	}
    
    currentSegment_ = (int)(name.rbegin()->toSegment());
    currentSegment_++;  // segments are zero-indexed
    if (currentSegment_ == totalSegment_) {
        cout << "Last segment received." << endl;
    } else {
        Name newInterestName(name.getPrefix(name.size() - 1));
        newInterestName.appendSegment((uint64_t)currentSegment_);
        Interest newInterest(newInterestName);
        
        face_.expressInterest
          (newInterest, bind(&Handler::onFileData, this, _1, _2), 
           bind(&Handler::onTimeout, this, _1));
    }
}

void Handler::onTimeout(const ptr_lib::shared_ptr<const Interest>& interest) {
    cout << "Timeout " << interest->getName().toUri() << endl;
    done_ = true;
}

void Handler::onVerified(const ptr_lib::shared_ptr<Data>& data)
{
    cout << "Signature verification: VERIFIED" << endl;
}

void Handler::onVerifyFailed(const ptr_lib::shared_ptr<Data>& data)
{
    cout << "Signature verification: FAILED" << endl;
}