#ifndef persister_h
#define persister_h

#include <fcntl.h>
#include <mutex>
#include <iostream>
#include <fstream>
#include <set>
#include "rpc.h"
#include "extent_server.h"

#define MAX_LOG_SZ 131072

namespace act {

class action {
public:
    int type;
    action(int type0) : type(type0) {}
    ~action() {};
    virtual void perform(extent_server *es) = 0;
};

class create_action : public action {
public:
    uint32_t type;
    create_action(uint32_t type0) :
        action(0), type(type0) {}
    ~create_action();

    void perform(extent_server *es) {
        extent_protocol::extentid_t id;
        es->create(type, id, false);
    }
};

class put_action : public action {
public:
    std::string buf;
    extent_protocol::extentid_t eid;
    put_action(extent_protocol::extentid_t eid0, std::string buf0) :
        action(1), eid(eid0), buf(buf0) {
            //传递string不能用string.c_str()+len的形式，会出错。但是why?
        }
    ~put_action();

    void perform(extent_server *es) {
        int r;
        es->put(eid, buf, r, false);
    }
};

class remove_action : public action {
public:
    extent_protocol::extentid_t eid;
    remove_action(extent_protocol::extentid_t eid0) :
        action(2), eid(eid0) {}
    ~remove_action();

    void perform(extent_server *es) {
        int r;
        es->remove(eid, r, false);
    }
};

}


/*
 * Your code here for Lab2A:
 * Implement class chfs_command, you may need to add command types such as
 * 'create', 'put' here to represent different commands a transaction requires. 
 * 
 * Here are some tips:
 * 1. each transaction in ChFS consists of several chfs_commands.
 * 2. each transaction in ChFS MUST contain a BEGIN command and a COMMIT command.
 * 3. each chfs_commands contains transaction ID, command type, and other information.
 * 4. you can treat a chfs_command as a log entry.
 */
class chfs_command {
public:
    typedef unsigned long long txid_t;
    enum cmd_type {
        CMD_BEGIN = 0,
        CMD_COMMIT,
        CMD_ABORT,
        CMD_CREATE,
        CMD_PUT,
        CMD_REMOVE
    };

    cmd_type type = CMD_BEGIN;
    txid_t id = 0;

    act::action *redo_act = nullptr, *undo_act = nullptr;
    //这两个变量存储的是内存地址，不可写入log
    
    // constructor
    chfs_command(cmd_type type0, txid_t id0, act::action *redo_act0 = nullptr, act::action *undo_act0 = nullptr) :
                 type(type0), id(id0), redo_act(redo_act0), undo_act(undo_act0) {}
    ~chfs_command() {
        // if (redo_act) delete redo_act;
        // if (undo_act) delete undo_act;

        //如有复制对象的析构，则指针指向的对象也会被析构，被复制对象无法再引用
        //删除对象时手动析构这部分吧！

        //（看看智能指针  
    }

    uint64_t size() const {
        uint64_t s = sizeof(cmd_type) + sizeof(txid_t);
        if (type == CMD_CREATE) {
            s += sizeof(uint32_t);
        } else if (type == CMD_REMOVE) {
            s += sizeof(extent_protocol::extentid_t);
        } else if (type == CMD_PUT) {
            s += sizeof(extent_protocol::extentid_t) + sizeof(size_t)
                + ((act::put_action *)redo_act)->buf.size();
        }
        return s;
    }

    std::string transfer() const {
        
        char log[MAX_LOG_SZ];
        int offset = 0;

        // std::cout << "log entry size: " << size() << std::endl;

        *(cmd_type *)(log+offset) = type; offset += sizeof(cmd_type);
        *(txid_t *)(log+offset) = id; offset += sizeof(txid_t);

        if (type == CMD_CREATE) {
            *(uint32_t *)(log+offset) = ((act::create_action *)redo_act)->type; 
            // std::cout << "type: " << ((act::create_action *)redo_act)->type << std::endl;
            offset += sizeof(uint32_t);
            // *(extent_protocol::extentid_t *)(log+offset) = ((act::remove_action *)undo_act)->eid; 
            // std::cout << "eid: " << ((act::remove_action *)undo_act)->eid << std::endl;
            // offset += sizeof(extent_protocol::extentid_t);

        } else if (type == CMD_REMOVE) {
            *(extent_protocol::extentid_t *)(log+offset) = ((act::remove_action *)redo_act)->eid; 
            offset += sizeof(extent_protocol::extentid_t);
            // *(uint32_t *)(log+offset) = ((act::create_action *)undo_act)->type; 
            // offset += sizeof(uint32_t);

        } else if (type == CMD_PUT) {
            *(extent_protocol::extentid_t *)(log+offset) = ((act::put_action *)redo_act)->eid;
            offset += sizeof(extent_protocol::extentid_t);
            *(size_t *)(log+offset) = ((act::put_action *)redo_act)->buf.size();
            offset += sizeof(size_t);
            // // std::cout << "string: " << ((act::put_action *)redo_act)->buf << std::endl;
            memcpy(log+offset, ((act::put_action *)redo_act)->buf.c_str(), ((act::put_action *)redo_act)->buf.size());
            offset += ((act::put_action *)redo_act)->buf.size();

            // *(extent_protocol::extentid_t *)(log+offset) = ((act::put_action *)undo_act)->eid;
            // offset += sizeof(extent_protocol::extentid_t);
            // *(size_t *)(log+offset) = ((act::put_action *)undo_act)->buf.size();
            // offset += sizeof(size_t);
            // // std::cout << "string: " << ((act::put_action *)undo_act)->buf << std::endl;
            // memcpy(log+offset, ((act::put_action *)undo_act)->buf.c_str(), ((act::put_action *)undo_act)->buf.size());
            // offset += ((act::put_action *)undo_act)->buf.size();
        }

        // delete [] log; <----不能在这里释放空间，下面的return要用到！！！
        
        return std::string(log, size());
    }
};

/*
 * Your code here for Lab2A:
 * Implement class persister. A persister directly interacts with log files.
 * Remember it should not contain any transaction logic, its only job is to 
 * persist and recover data.
 * 
 * P.S. When and how to do checkpoint is up to you. Just keep your logfile size
 *      under MAX_LOG_SZ and checkpoint file size under DISK_SIZE.
 */
template<typename command>
class persister {

public:
    persister(const std::string& file_dir);
    ~persister();

    // persist data into solid binary file
    // You may modify parameters in these functions
    void append_log(const command& log);
    void checkpoint();

    // restore data from solid binary file
    // You may modify parameters in these functions
    void restore_logdata(extent_server *es, chfs_command::txid_t &txid);
    void restore_checkpoint();

private:
    std::mutex mtx;
    std::string file_dir;
    std::string file_path_checkpoint;
    std::string file_path_logfile;

    // restored log data
    std::vector<command> log_entries;
};

template<typename command>
persister<command>::persister(const std::string& dir){
    // DO NOT change the file names here
    // // std::cout << "222222" << std::endl;
    file_dir = dir;
    file_path_checkpoint = file_dir + "/checkpoint.bin";
    file_path_logfile = file_dir + "/logdata.bin";


    // // std::cout << "111111" << std::endl;
    // // std::cout << file_path_logfile << std::endl;

    // outFile.open(file_path_logfile, std::ios::binary | std::ios::app);
}

template<typename command>
persister<command>::~persister() {
    // Your code here for lab2A
    // outFile.close();
}

template<typename command>
void persister<command>::append_log(const command& log) {
    
    // Your code here for lab2A
    
    //open和write必须要在同一个函数中，但是why？
    std::ofstream outFile(file_path_logfile, std::ios::binary | std::ios::app);
    std::cout << file_path_logfile << std::endl;
    if (!outFile) std::cout << "open file error!!!\n";
    
    std::string log_str = log.transfer();
    std::cout << log_str << std::endl;
    // int a = 1;
    // outFile.write((char *)&a, sizeof(int));
    outFile.write((char *)log_str.c_str(), log_str.size());

    outFile.close();

}

template<typename command>
void persister<command>::checkpoint() {
    // Your code here for lab2A

}

template<typename command>
void persister<command>::restore_logdata(extent_server *es, chfs_command::txid_t &txid) {

    std::ifstream inFile(file_path_logfile, std::ios::in | std::ios::binary);
    if (!inFile) return;

    chfs_command::cmd_type ty;

    //从log的最前面向后检查，如遇到create/put/remove类型就加入vector执行redo动作
    while (inFile.read((char *)&ty, sizeof(chfs_command::cmd_type))) {

        // std::cout << "cmd type: " << ty << std::endl;
        chfs_command::txid_t id;
        inFile.read((char *)&id, sizeof(chfs_command::txid_t));
        // std::cout << "tx id: " << id << std::endl;

        //vector的push_back是复制对象，如对象中有指针，则指针指向的位置可能被析构两次
        //可为什么这样写，析构也不行呢？
        log_entries.push_back(chfs_command(ty, id));

        //switch的不同case不能定义相同的变量，但是why?
        if (ty == chfs_command::CMD_CREATE) {
            uint32_t type;
            extent_protocol::extentid_t eid;
            inFile.read((char *)&type, sizeof(uint32_t));
            // std::cout << "create type: " << type << std::endl;
            // inFile.read((char *)&eid, sizeof(extent_protocol::extentid_t));
            // std::cout << "remove eid: " << eid << std::endl;
            // log_entries.push_back(chfs_command(ty, id, new act::create_action(type), new act::remove_action(eid)));
            log_entries.back().redo_act = new act::create_action(type);
            // log_entries.back().undo_act = new act::remove_action(eid);

        } else if (ty == chfs_command::CMD_REMOVE) {
            uint32_t type;
            extent_protocol::extentid_t eid;
            inFile.read((char *)&eid, sizeof(extent_protocol::extentid_t));
            // std::cout << "remove eid: " << eid << std::endl;
            // inFile.read((char *)&type, sizeof(uint32_t));
            // std::cout << "create type: " << type << std::endl;
            // log_entries.push_back(chfs_command(ty, id, new act::remove_action(eid), new act::create_action(type)));
            log_entries.back().redo_act = new act::remove_action(eid);
            // log_entries.back().undo_act = new act::create_action(type);

        } else if (ty == chfs_command::CMD_PUT) {
            extent_protocol::extentid_t eid;
            size_t len;

            inFile.read((char *)&eid, sizeof(extent_protocol::extentid_t));
            // std::cout << "put eid: " << eid << std::endl;
            inFile.read((char *)&len, sizeof(size_t));
            // std::cout << "string len: " << len << std::endl;
            if (len) {
                char *buf = new char [len];
                inFile.read((char *)buf, len);
                // // std::cout << "redo string: " << std::string(buf, len) << std::endl;
                log_entries.back().redo_act = new act::put_action(eid, std::string(buf, len));
                delete [] buf;
            } else {
                // std::cout << "empty string" << std::endl;
                log_entries.back().redo_act = new act::put_action(eid, std::string(""));
            }
            

            // inFile.read((char *)&eid, sizeof(extent_protocol::extentid_t));
            // // std::cout << "put eid: " << eid << std::endl;
            // inFile.read((char *)&len, sizeof(size_t));
            // // std::cout << "string len: " << len << std::endl;
            // if (len) {
            //     char *buf = new char [len];
            //     inFile.read((char *)buf, len);
            //     // // std::cout << "undo string: " << std::string(buf, len) << std::endl;
            //     log_entries.back().undo_act = new act::put_action(eid, std::string(buf, len));
            //     delete [] buf;
            // } else {
            //     // std::cout << "empty string" << std::endl;
            //     log_entries.back().undo_act = new act::put_action(eid, std::string(""));
            //     // std::cout << "empty string222" << std::endl;
            // }
        }
        // std::cout << std::endl;
    }
    inFile.close();

    std::set<chfs_command::txid_t> commit_set;
    for (chfs_command cmd : log_entries) {
        if (cmd.type == chfs_command::CMD_COMMIT) {
            commit_set.insert(cmd.id);
        } else if (cmd.type == chfs_command::CMD_BEGIN) {
            txid = cmd.id > txid ? cmd.id : txid;
        }
    }
    for (chfs_command cmd : log_entries) {
        if ((cmd.type == chfs_command::CMD_CREATE ||
            cmd.type == chfs_command::CMD_PUT ||
            cmd.type == chfs_command::CMD_REMOVE) &&
            commit_set.count(cmd.id)) {
                cmd.redo_act->perform(es);
            }
        
    }

    //restore后释放掉log_entry的内存
    for (chfs_command cmd : log_entries) {
        if (cmd.redo_act) delete cmd.redo_act;
        if (cmd.undo_act) delete cmd.undo_act;
    }
    log_entries.clear();
    ++txid;

};

template<typename command>
void persister<command>::restore_checkpoint() {
    // Your code here for lab2A

};

using chfs_persister = persister<chfs_command>;

#endif // persister_h