// chfs client.  implements FS operations using extent and lock server
#include "chfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

chfs_client::chfs_client()
{
    ec = new extent_client();

}

chfs_client::chfs_client(std::string extent_dst, std::string lock_dst)
{
    ec = new extent_client();
    if (ec->put(1, "") != extent_protocol::OK)
        printf("error init root dir\n"); // XYB: init root dir
}

chfs_client::inum
chfs_client::n2i(std::string n)
{
    std::istringstream ist(n);
    unsigned long long finum;
    ist >> finum;
    return finum;
}

std::string
chfs_client::filename(inum inum)
{
    std::ostringstream ost;
    ost << inum;
    return ost.str();
}

bool
chfs_client::isfile(inum inum)
{
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_FILE) {
        printf("isfile: %lld is a file\n", inum);
        return true;
    } 
    printf("isfile: %lld is a dir\n", inum);
    return false;
}
/** Your code here for Lab...
 * You may need to add routines such as
 * readlink, issymlink here to implement symbolic link.
 * 
 * */

bool
chfs_client::isdir(inum inum)
{
    // Oops! is this still correct when you implement symlink?
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }
    return (a.type == extent_protocol::T_DIR);
}

int
chfs_client::getfile(inum inum, fileinfo &fin)
{
    int r = OK;

    printf("getfile %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    printf("getfile %016llx -> sz %llu\n", inum, fin.size);

release:
    return r;
}

int
chfs_client::getdir(inum inum, dirinfo &din)
{
    int r = OK;

    printf("getdir %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;

release:
    return r;
}


#define EXT_RPC(xx) do { \
    if ((xx) != extent_protocol::OK) { \
        printf("EXT_RPC Error: %s:%d \n", __FILE__, __LINE__); \
        r = IOERR; \
        goto release; \
    } \
} while (0)

// Only support set size of attr
int
chfs_client::setattr(inum ino, size_t size)
{
    int r = OK;

    std::string content;
    extent_protocol::attr a;
    ec->getattr(ino, a);

    if (size == a.size) return r;
    else if (size > a.size) {
        ec->get(ino, content);
        std::string extra_string(size - a.size, '\0');
        content.append(extra_string);
        ec->put(ino, content);
    } else {
        ec->get(ino, content);
        content = content.substr(0, size);
        ec->put(ino, content);
    }
    /*
     * your code goes here.
     * note: get the content of inode ino, and modify its content
     * according to the size (<, =, or >) content length.
     */

    return r;
}

int
chfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;

    /*
     * your code goes here.
     * note: lookup is what you need to check if file exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */

    //没有考虑操作失败的情况
    // std::cout << parent << ' ' << std::string(name) << std::endl;

    //检查文件是否已经存在，如存在返回EXIST
    bool is_exist = false; inum ino;
    lookup(parent, name, is_exist, ino);
    if (is_exist) {
        return EXIST;
    }

    // std::cout << parent << ' ' << std::string(name) << std::endl;

    ec->create(extent_protocol::T_FILE, ino_out); //最好检查一下操作是否成功

    std::string dir;
    ec->get(parent, dir);

    char dir_entry[ENTRY_SIZE] = {0};
    strcpy(dir_entry, name);
    *(inum *)(dir_entry + ENTRY_SIZE - 8) = ino_out;

    dir.append(dir_entry, ENTRY_SIZE);
    ec->put(parent, dir);
    // std::cout << dir << std::endl;

    // std::cout << "new ino: " << ino_out << std::endl;

    // for (int i = 0; i < dir.size(); i += ENTRY_SIZE) {
    //     std::cout << *(inum *)(dir.c_str() + i + ENTRY_SIZE - 8) << ' ';
    // }
    // std::cout << std::endl;


    return r;
}

int
chfs_client::mkdir(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;

    /*
     * your code goes here.
     * note: lookup is what you need to check if directory exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */
    bool found;
    lookup(parent, name, found, ino_out);
    if (found) {
        r = EXIST;
        return r;
    }

    ec->create(extent_protocol::T_DIR, ino_out); 

    std::string dir;
    ec->get(parent, dir);

    char dir_entry[ENTRY_SIZE] = {0};
    strcpy(dir_entry, name);
    *(inum *)(dir_entry + ENTRY_SIZE - 8) = ino_out;

    dir.append(dir_entry, ENTRY_SIZE);
    ec->put(parent, dir);


    return r;
}

int
chfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    int r = OK;

    /*
     * your code goes here.
     * note: lookup file from parent dir according to name;
     * you should design the format of directory content.
     */
    std::string dir;
    ec->get(parent, dir);
    std::string filename(name);
    // std::cout << "look up filename: " << name << std::endl;

    found = false;

    for (int i = 0; i < dir.size(); i += ENTRY_SIZE) {
        // std::cout << dir.substr(i, filename.size()) << std::endl;
        // std::cout << std::string(filename).substr(0, ENTRY_SIZE - 8) << std::endl;
        if (dir.compare(i, filename.size(), filename) == 0 && dir[i + filename.size()] == '\0') {
            found = true;
            ino_out = *(inum *)(dir.c_str() + i + ENTRY_SIZE - 8);
            // std::cout << "found ino: " << ino_out << std::endl;
            // std::cout << dir.substr(i, filename.size()) << std::endl;
            break;
        }
    }
    // std::cout << found << std::endl;

    return r;
}

int
chfs_client::readdir(inum dir, std::list<dirent> &list)
{
    int r = OK;

    /*
     * your code goes here.
     * note: you should parse the dirctory content using your defined format,
     * and push the dirents to the list.
     */
    std::string dir_list;
    ec->get(dir, dir_list);

    for (int i = 0; i < dir_list.size(); i += ENTRY_SIZE) {
        std::string filename(dir_list.c_str() + i);
        dirent dir_entry;
        dir_entry.name = filename;
        dir_entry.inum = *(inum *)(dir_list.c_str() + i + ENTRY_SIZE - 8);
        // std::cout << dir_entry.name << ' ' << dir_entry.inum << std::endl;
        list.push_back(dir_entry);
    }

    return r;
}

int
chfs_client::read(inum ino, size_t size, off_t off, std::string &data)
{
    int r = OK;

    std::string content;
    extent_protocol::attr a;
    ec->getattr(ino, a);

    if (off >= a.size) {
        data = "";
        return r;
    } else {
        ec->get(ino, content);
        if (off + size <= a.size) {
            data = content.substr(off, size);
        } else {
            data = content.substr(off, a.size - off);
        }
    }

    /*
     * your code goes here.
     * note: read using ec->get().
     */

    return r;
}

int
chfs_client::write(inum ino, size_t size, off_t off, const char *data,
        size_t &bytes_written)
{
    //bytes_written是干什么的？

    int r = OK;

    std::string content;
    extent_protocol::attr a;
    ec->getattr(ino, a);

    if (off > a.size) {
        setattr(ino, off);
    }
    ec->get(ino, content);
    // std::cout << "content: " << content << std::endl;
    // std::cout << "write data: " << std::string(data, size) << std::endl;

    if (off + size < a.size) {
        content = content.substr(0, off) 
            + std::string(data, size) 
            + content.substr(off + size, a.size - off - size);
    } else {
        content = content.substr(0, off)
            + std::string(data, size);
    }
    ec->put(ino, content);
    bytes_written = size;
    /*
     * your code goes here.
     * note: write using ec->put().
     * when off > length of original file, fill the holes with '\0'.
     */

    return r;
}

int chfs_client::unlink(inum parent,const char *name)
{
    int r = OK;

    //检查是否有该文件
    bool found;
    inum ino;
    lookup(parent, name, found, ino);
    if (!found) {
        r = NOENT;
        return r;
    }
    //检查该文件是否为目录
    extent_protocol::attr a;
    ec->getattr(ino, a);
    if (a.type == extent_protocol::T_DIR) {
        r = NOTEMPTY;
        return r;
    }


    //在目录中删除entry
    std::string dir;
    ec->get(parent, dir);

    // std::string filename(name);
    // filename.push_back('\0');  //为避免-2与-20混淆
    size_t entry_pos = dir.find(name);
    dir.erase(entry_pos, ENTRY_SIZE);

    ec->put(parent, dir);

    //删除文件
    ec->remove(ino);

    /*
     * your code goes here.
     * note: you should remove the file using ec->remove,
     * and update the parent directory content.
     */

    return r;
}

int chfs_client::symlink(const char *link, inum parent, const char * name, inum &ino)
{
    int r = OK;

    // bool found;
    // lookup(parent, name, found, ino_out);
    // if (found) {
    //     r = EXIST;
    //     return r;
    // }

    ec->create(extent_protocol::T_LINK, ino); 
    ec->put(ino, std::string(link));
    std::cout << "1\n";

    std::string dir;
    ec->get(parent, dir);
    std::cout << "2\n";

    char dir_entry[ENTRY_SIZE] = {0};
    strcpy(dir_entry, name);
    *(inum *)(dir_entry + ENTRY_SIZE - 8) = ino;
    std::cout << "3\n";

    dir.append(dir_entry, ENTRY_SIZE);
    ec->put(parent, dir);
    std::cout << "4\n";


    return r;

}

int chfs_client::readlink(inum ino, std::string &link)
{
    ec->get(ino, link);
}

//lookup需要显式调用吗？ fuse会自己处理吧？

