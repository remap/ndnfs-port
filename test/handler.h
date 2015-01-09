#ifndef HANDLER_H
#define HANDLER_H

#include <ndn-cpp/common.hpp>
#include <ndn-cpp/data.hpp>
#include <ndn-cpp/interest.hpp>
#include <ndn-cpp/face.hpp>

class Handler {
public:
  Handler();
  ~Handler();
  
  void onData(const ndn::ptr_lib::shared_ptr<const ndn::Interest>&, const ndn::ptr_lib::shared_ptr<ndn::Data>&);
  void onTimeout(const ndn::ptr_lib::shared_ptr<const ndn::Interest>&);
private:
  bool done_;
  
};

#endif