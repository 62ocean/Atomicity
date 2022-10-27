#include "inode_manager.h"

// disk layer -----------------------------------------

disk::disk()
{
  bzero(blocks, sizeof(blocks));
}

void
disk::read_block(blockid_t id, char *buf)
{
  for (int i = 0; i < BLOCK_SIZE; ++i) {
    buf[i] = blocks[id][i];
  }
}

void
disk::write_block(blockid_t id, const char *buf)
{
  for (int i = 0; i < BLOCK_SIZE; ++i) {
    blocks[id][i] = buf[i];
  }
}

// block layer -----------------------------------------

bool
block_manager::isfree_block(blockid_t blockid, char *buf)
{
  blockid_t bit_id = blockid % BPB;
  if ((buf[bit_id / 8] >> (7 - (bit_id % 8))) & 0x01) {
    return false;
  } else {
    return true;
  }
}

void
block_manager::setbit_block(blockid_t blockid, char *buf)
{
  blockid_t bit_id = blockid % BPB;
  buf[bit_id / 8] |= (0x01 << (7 - (bit_id % 8)));
}
void
block_manager::freebit_block(blockid_t blockid, char *buf)
{
  // std::cout << "free block: " << blockid << std::endl;
  blockid_t bit_id = blockid % BPB;
  buf[bit_id / 8] &= ~(0x01 << (7 - (bit_id % 8)));
}


// Allocate a free disk block.
blockid_t
block_manager::alloc_block()
{
  /*
   * your code goes here.
   * note: you should mark the corresponding bit in block bitmap when alloc.
   * you need to think about which block you can start to be allocated.
   */
  char buf[BLOCK_SIZE];
  blockid_t bitblock = 0, block_id = 0;

  for (int i = DATA_BLOCK0; i < BLOCK_NUM; ++i) {
    if (bitblock != BBLOCK(i)) {
      bitblock = BBLOCK(i);
      read_block(bitblock, buf);
    }
    if (isfree_block(i, buf)) {
      block_id = i;
      setbit_block(i, buf);
      write_block(bitblock, buf);
      break;
    }
  }
  // std::cout << "alloc blockid: " << block_id << std::endl;

  return block_id;
}

void
block_manager::free_block(uint32_t id)
{
  /* 
   * your code goes here.
   * note: you should unmark the corresponding bit in the block bitmap when free.
   */
//需不需要判断block已经是free的情况？

  char buf[BLOCK_SIZE];
  read_block(BBLOCK(id), buf);
  freebit_block(id, buf);
  write_block(BBLOCK(id), buf);
  
  return;
}

// The layout of disk should be like this:
// |<-sb->|<-free block bitmap->|<-inode table->|<-data->|
block_manager::block_manager()
{
  d = new disk();

  // format the disk
  sb.size = BLOCK_SIZE * BLOCK_NUM;
  sb.nblocks = BLOCK_NUM;
  sb.ninodes = INODE_NUM;

}

void
block_manager::read_block(uint32_t id, char *buf)
{
  // std::cout << "read block: " << id << std::endl;
  d->read_block(id, buf);
}

void
block_manager::write_block(uint32_t id, const char *buf)
{
  // std::cout << "write block: " << id << std::endl;
  d->write_block(id, buf);
}

// inode layer -----------------------------------------

inode_manager::inode_manager()
{
  bm = new block_manager();
  uint32_t root_dir = alloc_inode(extent_protocol::T_DIR);
  if (root_dir != 1) {
    printf("\tim: error! alloc first inode %d, should be 1\n", root_dir);
    exit(0);
  }
}

blockid_t
inode_manager::indirect_blockid(int index, char *buf)
{
  return *(blockid_t *)(buf + (index - NDIRECT)*sizeof(uint));
}
void
inode_manager::set_indirect_blockid(int index, blockid_t newid, char *buf)
{
  *(blockid_t *)(buf + (index - NDIRECT)*sizeof(uint)) = newid;
}

/* Create a new file.
 * Return its inum. */
uint32_t
inode_manager::alloc_inode(uint32_t type)
{
  /* 
   * your code goes here.
   * note: the normal inode block should begin from the 2nd inode block.
   * the 1st is used for root_dir, see inode_manager::inode_manager().
   */
  char buf[BLOCK_SIZE];
  blockid_t block_id = 0, bitblock = 0;
  uint32_t inode_id = 0;

  for (int i = 1; i <= INODE_NUM; ++i) {
    block_id = IBLOCK(i, bm->sb.nblocks);

    if (BBLOCK(block_id) != bitblock) {
      bitblock = BBLOCK(block_id);
      bm->read_block(bitblock, buf);
    }
    // std::cout << "if inode bit free: " << bm->isfree_block(block_id, buf) << std::endl;
    if (bm->isfree_block(block_id, buf)) {
      inode_id = i;
      bm->setbit_block(block_id, buf);
      bm->write_block(bitblock, buf);
      break;
    }
  }


  if (inode_id) {
    inode_t inode;
    inode.type = type;
    inode.size = 0;
    put_inode(inode_id, &inode);
  }

  return inode_id;
}

void
inode_manager::free_inode(uint32_t inum)
{
  /* 
   * your code goes here.
   * note: you need to check if the inode is already a freed one;
   * if not, clear it, and remember to write back to disk.
   */

  blockid_t block_id = IBLOCK(inum, bm->sb.nblocks);
  char buf[BLOCK_SIZE];
  bm->read_block(BBLOCK(block_id), buf);

  if (bm->isfree_block(block_id, buf)) return;
  else {
    bm->freebit_block(block_id, buf);
    bm->write_block(BBLOCK(block_id), buf);
  }
  // std::cout << "free inode:\n";
  // std::cout << "if inode bit free: " << bm->isfree_block(block_id, buf) << std::endl;

  return;
}


/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
//什么情况下会返回null? 这个函数需要检查inode是否为free吗？
struct inode* 
inode_manager::get_inode(uint32_t inum)
{
  blockid_t bitblock_id = IBLOCK(inum, bm->sb.nblocks);
  char bitblock[BLOCK_SIZE];
  bm->read_block(BBLOCK(bitblock_id), bitblock);
  // std::cout << "if inode bit free: " << bm->isfree_block(bitblock_id, bitblock) << std::endl;
  if (bm->isfree_block(bitblock_id, bitblock)) return nullptr;

  struct inode *ino;
  char *buf = new char [BLOCK_SIZE];
  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  ino = (inode_t *)buf + inum%IPB;

  return ino;
}

void
inode_manager::put_inode(uint32_t inum, struct inode *ino)
{
  char buf[BLOCK_SIZE];
  struct inode *ino_disk;

  printf("\tim: put_inode %d\n", inum);
  if (ino == NULL)
    return;

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  ino_disk = (struct inode*)buf + inum%IPB;
  *ino_disk = *ino;
  bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
}

#define MIN(a,b) ((a)<(b) ? (a) : (b))

/* Get all the data of a file by inum. 
 * Return alloced data, should be freed by caller. */
void
inode_manager::read_file(uint32_t inum, char **buf_out, int *size)
{
  inode_t *ino = get_inode(inum);
  *size = ino->size;
  if (*size == 0) return;

  int block_num = (ino->size - 1) / BLOCK_SIZE + 1;

  (*buf_out) = new char [block_num * BLOCK_SIZE];
  for (int i = 0; i < MIN(block_num, NDIRECT); ++i) {
    bm->read_block(ino->blocks[i], (*buf_out)+i*BLOCK_SIZE);
  }

  char indirect_block[BLOCK_SIZE];
  if (block_num > NDIRECT) {
    bm->read_block(ino->blocks[NDIRECT], indirect_block);
    // std::cout << "read indirect block: " << ino->blocks[NDIRECT] << std::endl;
    for (int i = NDIRECT; i < block_num; ++i) {
      bm->read_block(indirect_blockid(i, indirect_block), (*buf_out)+i*BLOCK_SIZE);
    }
  }

  ino->atime = time(0);
  put_inode(inum, ino);

  
  /*
   * your code goes here.
   * note: read blocks related to inode number inum,
   * and copy them to buf_out
   */
  delete ino;
  
  return;
}

/* alloc/free blocks if needed */
void
inode_manager::write_file(uint32_t inum, const char *buf, int size)
{
  inode_t *ino = get_inode(inum);
  int block_num = size == 0 ? 0 : ((size - 1) / BLOCK_SIZE + 1);


  //free blocks
  char indirect_block[BLOCK_SIZE];
  if (ino->size / BLOCK_SIZE + 1 > NDIRECT) {
    // std::cout << "111\n";
    bm->read_block(ino->blocks[NDIRECT], indirect_block);
  }
  int old_block_num = ino->size == 0 ? 0 : ((ino->size - 1) / BLOCK_SIZE + 1);
  for (int i = block_num; i < old_block_num; ++i) {
    // std::cout << "111\n";
    if (i < NDIRECT) bm->free_block(ino->blocks[i]);
    else bm->free_block(indirect_blockid(i, indirect_block));
  }
  if (block_num <= NDIRECT && ino->size / BLOCK_SIZE + 1 > NDIRECT) {
    // std::cout << "111\n";
    bm->free_block(ino->blocks[NDIRECT]);
  }

  // std::cout << "after free\n";

  

  //direct blocks
  for (int i = 0; i < MIN(block_num, NDIRECT); ++i) {
    if (i * BLOCK_SIZE >= ino->size) {
      blockid_t new_block = bm->alloc_block();
      // std::cout << "new block id: " << new_block << std::endl;
      ino->blocks[i] = new_block;
    }
    bm->write_block(ino->blocks[i], buf+i*BLOCK_SIZE);
    // std::cout << "write block id: " << ino->blocks[i] << std::endl;
  }

  // std::cout << "after direct\n";

  //indirect blocks
  if (block_num > NDIRECT) {
    // std::cout << "indirect block used\n";
    if (ino->size <= NDIRECT * BLOCK_SIZE) {
      ino->blocks[NDIRECT] = bm->alloc_block();
    }
    char indirect_block[BLOCK_SIZE];
    bm->read_block(ino->blocks[NDIRECT], indirect_block);
    for (int i = NDIRECT; i < block_num; ++i) {
      if (i * BLOCK_SIZE >= ino->size) {
        blockid_t new_block = bm->alloc_block();
        set_indirect_blockid(i, new_block, indirect_block);
      }
      bm->write_block(indirect_blockid(i, indirect_block), buf+i*BLOCK_SIZE);
    }
    bm->write_block(ino->blocks[NDIRECT], indirect_block);
  }
  // std::cout << "after indirect\n";

  ino->size = size;
  ino->mtime = time(0);
  ino->ctime = time(0);
  // std::cout << "mtime: " << time(0) << std::endl;
  put_inode(inum, ino);

  delete ino;
  
  /*
   * your code goes here.
   * note: write buf to blocks of inode inum.
   * you need to consider the situation when the size of buf 
   * is larger or smaller than the size of original inode
   */
  
  return;
}

void
inode_manager::get_attr(uint32_t inum, extent_protocol::attr &a)
{
  /*
   * your code goes here.
   * note: get the attributes of inode inum.
   * you can refer to "struct attr" in extent_protocol.h
   */
  inode_t *inode = get_inode(inum);
  if (inode == nullptr) {
    a.type = 0;
    delete inode;
    return;
  }

  a.atime = inode->atime; 
  a.ctime = inode->ctime;
  a.mtime = inode->mtime;
  a.size = inode->size;
  a.type = inode->type;
  delete inode;
  
  return;
}

void
inode_manager::remove_file(uint32_t inum)
{
  /*
   * your code goes here
   * note: you need to consider about both the data block and inode of the file
   */
  inode_t *ino = get_inode(inum);
  int block_num = (ino->size == 0 ? 0 : ino->size - 1) / BLOCK_SIZE + 1;

  //free blocks
  for (int i = 0; i < MIN(block_num, NDIRECT); ++i) {
    bm->free_block(ino->blocks[i]);
  }
  if (block_num > NDIRECT) {
    char indirect_block[BLOCK_SIZE];
    bm->read_block(ino->blocks[NDIRECT], indirect_block);
    for (int i = NDIRECT; i < block_num; ++i) {
      bm->free_block(indirect_blockid(i, indirect_block));
    }
    bm->free_block(ino->blocks[NDIRECT]);
  }

  //free inode
  free_inode(inum);

  delete ino;
  
  return;
}
