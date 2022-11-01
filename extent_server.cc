// the extent server implementation

#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "extent_server.h"
#include "persister.h"

extent_server::extent_server() 
{
  im = new inode_manager();
  _persister = new chfs_persister("log"); // DO NOT change the dir name here
  
  // Your code here for Lab2A: recover data on startup
  _persister->restore_logdata(this, txid);
}

int extent_server::create(uint32_t type, extent_protocol::extentid_t &id, bool iflog)
{
  // prepare log entry
  if (iflog) {
    printf("log extent_server: create inode\n");
    chfs_command cmd(chfs_command::CMD_CREATE, txid);
    cmd.redo_act = new act::create_action(type);
    // cmd.undo_act = new act::remove_action(id);
    std::cout << cmd.type << ' ' << cmd.id << ' ' << cmd.size() << std::endl;
    std::cout << id << std::endl;
    _persister->append_log(cmd);

    if (cmd.redo_act) delete cmd.redo_act; 
    // delete cmd.undo_act;
  }

  // alloc a new inode and return inum
  printf("extent_server: create inode\n");
  id = im->alloc_inode(type);

  return extent_protocol::OK;
}

int extent_server::put(extent_protocol::extentid_t id, std::string buf, int &, bool iflog)
{
  // prepare log entry
  if (iflog) {
    printf("log extent_server: put %lld\n", id);
    std::string old_content;
    // get(id, old_content);
    chfs_command cmd(chfs_command::CMD_PUT, txid);
    cmd.redo_act = new act::put_action(id, buf);
    // cmd.undo_act = new act::put_action(id, old_content);
    std::cout << cmd.type << ' ' << cmd.id << ' ' << cmd.size() << std::endl;
    // std::cout << buf.size() << ' ' << buf << std::endl;
    
    _persister->append_log(cmd);

    if (cmd.redo_act) delete cmd.redo_act; 
    // delete cmd.undo_act;
    std::cout << "after log\n";
  }
  

  printf("extent_server: put %lld\n", id);
  // std::cout << buf << std::endl;
  id &= 0x7fffffff;
  
  const char * cbuf = buf.c_str();
  int size = buf.size();
  im->write_file(id, cbuf, size);
  
  return extent_protocol::OK;
}

int extent_server::get(extent_protocol::extentid_t id, std::string &buf)
{
  printf("extent_server: get %lld\n", id);

  id &= 0x7fffffff;

  int size = 0;
  char *cbuf = NULL;

  im->read_file(id, &cbuf, &size);
  if (size == 0)
    buf = "";
  else {
    buf.assign(cbuf, size);
    free(cbuf);
  }

  // std::cout << buf << std::endl;

  return extent_protocol::OK;
}

int extent_server::getattr(extent_protocol::extentid_t id, extent_protocol::attr &a)
{
  printf("extent_server: getattr %lld\n", id);

  id &= 0x7fffffff;
  
  extent_protocol::attr attr;
  memset(&attr, 0, sizeof(attr));
  im->get_attr(id, attr);
  a = attr;

  return extent_protocol::OK;
}

int extent_server::remove(extent_protocol::extentid_t id, int &, bool iflog)
{
  // prepare log entry
  if (iflog) {
    printf("log extent_server: remove %lld\n", id);
    extent_protocol::attr attr;
    getattr(id, attr);
    chfs_command cmd(chfs_command::CMD_REMOVE, txid);
    cmd.redo_act = new act::remove_action(id);
    // cmd.undo_act = new act::create_action(attr.type);
    std::cout << cmd.type << ' ' << cmd.id << ' ' << cmd.size() << std::endl;
    _persister->append_log(cmd);

    if (cmd.redo_act) delete cmd.redo_act; 
    // delete cmd.undo_act;
  }
  

  printf("extent_server: remove %lld\n", id);

  id &= 0x7fffffff;
  im->remove_file(id);
 
  return extent_protocol::OK;
}

int extent_server::begin_tx()
{
  chfs_command cmd(chfs_command::CMD_BEGIN, txid);
  _persister->append_log(cmd);
  return extent_protocol::OK;
}

int extent_server::commit_tx()
{
  chfs_command cmd(chfs_command::CMD_COMMIT, txid);
  _persister->append_log(cmd);
  ++txid;
  return extent_protocol::OK;
}
