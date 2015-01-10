#ifndef HANDLER_H
#define HANDLER_H

#include <ndn-cpp/common.hpp>
#include <ndn-cpp/data.hpp>
#include <ndn-cpp/interest.hpp>
#include <ndn-cpp/face.hpp>

#include <ndn-cpp/security/identity/memory-identity-storage.hpp>
#include <ndn-cpp/security/identity/memory-private-key-storage.hpp>
#include <ndn-cpp/security/policy/no-verify-policy-manager.hpp>

class Handler {
public:
  Handler
    (ndn::Face &face, ndn::KeyChain &keyChain, std::string nameStr, 
     std::string fileName = "", bool fetchFile = false, bool doVerification = true);
  
  ~Handler();
  
  void 
  onAttrData(const ndn::ptr_lib::shared_ptr<const ndn::Interest>&, const ndn::ptr_lib::shared_ptr<ndn::Data>&);
  
  void 
  onFileData (const ndn::ptr_lib::shared_ptr<const ndn::Interest>& interest, const ndn::ptr_lib::shared_ptr<ndn::Data>& data);
  
  void 
  onTimeout(const ndn::ptr_lib::shared_ptr<const ndn::Interest>&);
  
  void 
  onVerified(const ndn::ptr_lib::shared_ptr<ndn::Data>& data);
  
  void 
  onVerifyFailed(const ndn::ptr_lib::shared_ptr<ndn::Data>& data);
private:
  bool done_;
  bool fetchFile_;
  bool doVerification_;
  
  std::string fileName_;
  std::string nameStr_;
  int currentSegment_;
  int totalSegment_;
  
  ndn::Face& face_;
  ndn::KeyChain& keyChain_;
};

#endif