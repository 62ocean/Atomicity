// this is the extent server

#ifndef extent_server_h
#define extent_server_h

#include <string>
#include <map>
#include "extent_protocol.h"

#include "inode_manager.h"

template<typename command>
class persister;
class chfs_command;
using chfs_persister = persister<chfs_command>;

class extent_server {
 protected:
#if 0
  typedef struct extent {
    std::string data;
    struct extent_protocol::attr attr;
  } extent_t;
  std::map <extent_protocol::extentid_t, extent_t> extents;
#endif
  inode_manager *im;
  chfs_persister *_persister;

 public:
  typedef unsigned long long txid_t;
  txid_t txid = 0;

  extent_server();

  int checkpoint();
  int begin_tx();
  int commit_tx();
  int create(uint32_t type, extent_protocol::extentid_t &id, bool iflog = true);
  int put(extent_protocol::extentid_t id, std::string, int &, bool iflog = true);
  int get(extent_protocol::extentid_t id, std::string &);
  int getattr(extent_protocol::extentid_t id, extent_protocol::attr &);
  int remove(extent_protocol::extentid_t id, int &, bool iflog = true);

  // Your code here for lab2A: add logging APIs
};

#endif 







