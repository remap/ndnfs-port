#ifndef HANDLER_H
#define HANDLER_H

#include <unistd.h>

#include <ndn-cpp/common.hpp>
#include <ndn-cpp/data.hpp>
#include <ndn-cpp/interest.hpp>
#include <ndn-cpp/face.hpp>

#include <ndn-cpp/security/identity/memory-identity-storage.hpp>
#include <ndn-cpp/security/identity/memory-private-key-storage.hpp>
#include <ndn-cpp/security/policy/no-verify-policy-manager.hpp>

/**
 * Handler(client) class handles request from ndnfs client.
 * Currently requests include: fetch metadata and fetch whole file.
 */
class Handler {
public:
  /**
   * Handler constructor takes a face, its thread-safetiness should be handled in calling function.
   * @param face Face used for this handler request
   * @param keyChain Keychain used for verifying received data
   * @param nameStr Name string from the client's input, used by expressInterest
   * @param fileName Name of local file to save into when fetching
   * @param fetchFile Flag for fetching the entire file, or only the attribute
   * @param doVerification Flag for doing verification or not
   */
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