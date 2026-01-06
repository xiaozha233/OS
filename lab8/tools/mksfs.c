#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <assert.h>

typedef int bool;

#define __error(msg, quit, ...)                                                         \
    do {                                                                                \
        fprintf(stderr, #msg ": function %s - line %d: ", __FUNCTION__, __LINE__);      \
        if (errno != 0) {                                                               \
            fprintf(stderr, "[error] %s: ", strerror(errno));                           \
        }                                                                               \
        fprintf(stderr, "\n\t"), fprintf(stderr, __VA_ARGS__);                          \
        errno = 0;                                                                      \
        if (quit) {                                                                     \
            exit(-1);                                                                   \
        }                                                                               \
    } while (0)

#define warn(...)           __error(warn, 0, __VA_ARGS__)
#define bug(...)            __error(bug, 1, __VA_ARGS__)

#define static_assert(x)                                                                \
    switch (x) {case 0: case (x): ; }

/* 2^31 + 2^29 - 2^25 + 2^22 - 2^19 - 2^16 + 1 */
#define GOLDEN_RATIO_PRIME_32       0x9e370001UL

#define HASH_SHIFT                              10
#define HASH_LIST_SIZE                          (1 << HASH_SHIFT)

// __hash32 - 32位哈希函数
static inline uint32_t
__hash32(uint32_t val, unsigned int bits) {
    uint32_t hash = val * GOLDEN_RATIO_PRIME_32; // 黄金分割率散列
    return (hash >> (32 - bits));
}

// hash32 - 32位整数哈希
static uint32_t
hash32(uint32_t val) {
    return __hash32(val, HASH_SHIFT);
}

// hash64 - 64位整数哈希（取低32位）
static uint32_t
hash64(uint64_t val) {
    return __hash32((uint32_t)val, HASH_SHIFT);
}

// safe_malloc - 安全的内存分配，失败则退出
void *
safe_malloc(size_t size) {
    void *ret;
    if ((ret = malloc(size)) == NULL) {
        bug("malloc %lu bytes failed.\n", (long unsigned)size);
    }
    return ret;
}

// safe_strdup - 安全的字符串复制
char *
safe_strdup(const char *str) {
    char *ret;
    if ((ret = strdup(str)) == NULL) {
        bug("strdup failed: %s\n", str);
    }
    return ret;
}

// safe_stat - 安全的获取文件状态
struct stat *
safe_stat(const char *filename) {
    static struct stat __stat;
    if (stat(filename, &__stat) != 0) {
        bug("stat %s failed.\n", filename);
    }
    return &__stat;
}

// safe_fstat - 安全的获取文件描述符状态
struct stat *
safe_fstat(int fd) {
    static struct stat __stat;
    if (fstat(fd, &__stat) != 0) {
        bug("fstat %d failed.\n", fd);
    }
    return &__stat;
}

// safe_lstat - 安全的获取符号链接状态
struct stat *
safe_lstat(const char *name) {
    static struct stat __stat;
    if (lstat(name, &__stat) != 0) {
        bug("lstat '%s' failed.\n", name);
    }
    return &__stat;
}

// safe_fchdir - 安全的更改目录
void
safe_fchdir(int fd) {
    if (fchdir(fd) != 0) {
        bug("fchdir failed %d.\n", fd);
    }
}

#define SFS_MAGIC                               0x2f8dbe2a // SFS魔数
#define SFS_NDIRECT                             12 // 直接索引块数量
#define SFS_BLKSIZE                             4096                                    // 4K 块大小
#define SFS_MAX_NBLKS                           (1024UL * 512)                          // 4K * 512K 最大块数
#define SFS_MAX_INFO_LEN                        31 // 最大信息长度
#define SFS_MAX_FNAME_LEN                       255 // 最大文件名长度
#define SFS_MAX_FILE_SIZE                       (1024UL * 1024 * 128)                   // 128M 最大文件大小

#define SFS_BLKBITS                             (SFS_BLKSIZE * CHAR_BIT) // 每块比特数
#define SFS_TYPE_FILE                           1 // 文件类型
#define SFS_TYPE_DIR                            2 // 目录类型
#define SFS_TYPE_LINK                           3 // 链接类型

#define SFS_BLKN_SUPER                          0 // 超级块号
#define SFS_BLKN_ROOT                           1 // 根目录块号
#define SFS_BLKN_FREEMAP                        2 // 空闲映射起始块号

// 缓存块结构
struct cache_block {
    uint32_t ino; // 块号
    struct cache_block *hash_next; // 哈希表链表
    void *cache; // 缓存数据
};

// 缓存inode结构
struct cache_inode {
    struct inode {
        uint32_t size; // 文件大小
        uint16_t type; // 文件类型
        uint16_t nlinks; // 链接数
        uint32_t blocks; // 占用块数
        uint32_t direct[SFS_NDIRECT]; // 直接索引
        uint32_t indirect; // 一级间接索引
        uint32_t db_indirect; // 二级间接索引
    } inode;
    ino_t real; // 真实inode号（宿主机）
    uint32_t ino; // SFS inode号
    uint32_t nblks; // 块数
    struct cache_block *l1, *l2; // 一级和二级间接索引块缓存
    struct cache_inode *hash_next; // 哈希表链表
};

// SFS文件系统结构
struct sfs_fs {
    struct {
        uint32_t magic; // 魔数
        uint32_t blocks; // 总块数
        uint32_t unused_blocks; // 未使用块数
        char info[SFS_MAX_INFO_LEN + 1]; // 信息字符串
    } super; // 超级块
    struct subpath {
        struct subpath *next, *prev;
        char *subname;
    } __sp_nil, *sp_root, *sp_end; // 路径栈
    int imgfd; // 镜像文件描述符
    uint32_t ninos, next_ino; // inode总数，下一个可用inode
    struct cache_inode *root; // 根目录inode
    struct cache_inode *inodes[HASH_LIST_SIZE]; // inode哈希表
    struct cache_block *blocks[HASH_LIST_SIZE]; // 块哈希表
};

// SFS目录项结构
struct sfs_entry {
    uint32_t ino; // inode号
    char name[SFS_MAX_FNAME_LEN + 1]; // 文件名
};

// sfs_alloc_ino - 分配一个新的Block（用作inode或数据块）
static uint32_t
sfs_alloc_ino(struct sfs_fs *sfs) {
    if (sfs->next_ino < sfs->ninos) { // 如果还有空闲块
        sfs->super.unused_blocks --; // 减少未使用块计数
        return sfs->next_ino ++; // 返回分配的块号
    }
    bug("out of disk space.\n"); // 磁盘空间不足
}

// alloc_cache_block - 分配并初始化缓存块
static struct cache_block *
alloc_cache_block(struct sfs_fs *sfs, uint32_t ino) {
    struct cache_block *cb = safe_malloc(sizeof(struct cache_block));
    cb->ino = (ino != 0) ? ino : sfs_alloc_ino(sfs); // 分配块号
    cb->cache = memset(safe_malloc(SFS_BLKSIZE), 0, SFS_BLKSIZE); // 分配并清零缓存
    struct cache_block **head = sfs->blocks + hash32(ino);
    cb->hash_next = *head, *head = cb; // 插入哈希表
    return cb;
}

// search_cache_block - 在缓存中查找块
struct cache_block *
search_cache_block(struct sfs_fs *sfs, uint32_t ino) {
    struct cache_block *cb = sfs->blocks[hash32(ino)];
    while (cb != NULL && cb->ino != ino) { // 遍历链表
        cb = cb->hash_next;
    }
    return cb;
}

// alloc_cache_inode - 分配并初始化缓存inode
static struct cache_inode *
alloc_cache_inode(struct sfs_fs *sfs, ino_t real, uint32_t ino, uint16_t type) {
    struct cache_inode *ci = safe_malloc(sizeof(struct cache_inode));
    ci->ino = (ino != 0) ? ino : sfs_alloc_ino(sfs); // 分配块号
    ci->real = real, ci->nblks = 0, ci->l1 = ci->l2 = NULL;
    struct inode *inode = &(ci->inode);
    memset(inode, 0, sizeof(struct inode));
    inode->type = type; // 设置类型
    struct cache_inode **head = sfs->inodes + hash64(real);
    ci->hash_next = *head, *head = ci; // 插入哈希表
    return ci;
}

// search_cache_inode -根据真实inode查找缓存inode
struct cache_inode *
search_cache_inode(struct sfs_fs *sfs, ino_t real) {
    struct cache_inode *ci = sfs->inodes[hash64(real)];
    while (ci != NULL && ci->real != real) { // 遍历链表
        ci = ci->hash_next;
    }
    return ci;
}

// create_sfs - 创建SFS文件系统结构
struct sfs_fs *
create_sfs(int imgfd) {
    uint32_t ninos, next_ino;
    struct stat *stat = safe_fstat(imgfd);
    if ((ninos = stat->st_size / SFS_BLKSIZE) > SFS_MAX_NBLKS) { // 计算最大块数
        ninos = SFS_MAX_NBLKS;
        warn("img file is too big (%llu bytes, only use %u blocks).\n",
                (unsigned long long)stat->st_size, ninos);
    }
    // 计算Freemap之后的一个块号，确保img文件足够大能容纳基本结构
    if ((next_ino = SFS_BLKN_FREEMAP + (ninos + SFS_BLKBITS - 1) / SFS_BLKBITS) >= ninos) {
        bug("img file is too small (%llu bytes, %u blocks, bitmap use at least %u blocks).\n",
                (unsigned long long)stat->st_size, ninos, next_ino - 2);
    }

    struct sfs_fs *sfs = safe_malloc(sizeof(struct sfs_fs));
    sfs->super.magic = SFS_MAGIC;
    sfs->super.blocks = ninos, sfs->super.unused_blocks = ninos - next_ino; // 初始化超级块
    snprintf(sfs->super.info, SFS_MAX_INFO_LEN, "simple file system");

    sfs->ninos = ninos, sfs->next_ino = next_ino, sfs->imgfd = imgfd;
    sfs->sp_root = sfs->sp_end = &(sfs->__sp_nil);
    sfs->sp_end->prev = sfs->sp_end->next = NULL;

    int i;
    for (i = 0; i < HASH_LIST_SIZE; i ++) {
        sfs->inodes[i] = NULL;
        sfs->blocks[i] = NULL;
    }

    sfs->root = alloc_cache_inode(sfs, 0, SFS_BLKN_ROOT, SFS_TYPE_DIR); // 分配根目录inode
    return sfs;
}

// subpath_push - 压入子路径
static void
subpath_push(struct sfs_fs *sfs, const char *subname) {
    struct subpath *subpath = safe_malloc(sizeof(struct subpath));
    subpath->subname = safe_strdup(subname);
    sfs->sp_end->next = subpath;
    subpath->prev = sfs->sp_end;
    subpath->next = NULL;
    sfs->sp_end = subpath;
}

// subpath_pop - 弹出子路径
static void
subpath_pop(struct sfs_fs *sfs) {
    assert(sfs->sp_root != sfs->sp_end);
    struct subpath *subpath = sfs->sp_end;
    sfs->sp_end = sfs->sp_end->prev, sfs->sp_end->next = NULL;
    free(subpath->subname), free(subpath);
}

// subpath_show - 显示当前路径
static void
subpath_show(FILE *fout, struct sfs_fs *sfs, const char *name) {
    struct subpath *subpath = sfs->sp_root;
    fprintf(fout, "current is: /");
    while ((subpath = subpath->next) != NULL) {
        fprintf(fout, "%s/", subpath->subname);
    }
    if (name != NULL) {
        fprintf(fout, "%s", name);
    }
    fprintf(fout, "\n");
}

// write_block - 将数据写入镜像文件块
static void
write_block(struct sfs_fs *sfs, void *data, size_t len, uint32_t ino) {
    assert(len <= SFS_BLKSIZE && ino < sfs->ninos); // 检查参数
    static char buffer[SFS_BLKSIZE];
    if (len != SFS_BLKSIZE) { // 如果数据长度不足一块，填充0
        memset(buffer, 0, sizeof(buffer));
        data = memcpy(buffer, data, len);
    }
    off_t offset = (off_t)ino * SFS_BLKSIZE; // 计算偏移
    ssize_t ret;
    if ((ret = pwrite(sfs->imgfd, data, SFS_BLKSIZE, offset)) != SFS_BLKSIZE) { // 写入
        bug("write %u block failed: (%d/%d).\n", ino, (int)ret, SFS_BLKSIZE);
    }
}

// flush_cache_block - 刷新缓存块到磁盘
static void
flush_cache_block(struct sfs_fs *sfs, struct cache_block *cb) {
    write_block(sfs, cb->cache, SFS_BLKSIZE, cb->ino); // 将缓存内容写入磁盘块
}

// flush_cache_inode - 刷新缓存inode到磁盘
static void
flush_cache_inode(struct sfs_fs *sfs, struct cache_inode *ci) {
    write_block(sfs, &(ci->inode), sizeof(ci->inode), ci->ino); // 将inode结构写入磁盘块（SFS中inode占一个块）
}

// close_sfs - 关闭SFS并写入元数据
void
close_sfs(struct sfs_fs *sfs) {
    static char buffer[SFS_BLKSIZE];
    uint32_t i, j, ino = SFS_BLKN_FREEMAP;
    uint32_t ninos = sfs->ninos, next_ino = sfs->next_ino;
    // 写入Freemap（位图）
    for (i = 0; i < ninos; ino ++, i += SFS_BLKBITS) {
        memset(buffer, 0, sizeof(buffer));
        if (i + SFS_BLKBITS > next_ino) { // 如果这段位图包含已用/未用边界
            uint32_t start = 0, end = SFS_BLKBITS;
            // 计算需要标记为占用（0）或空闲（1）的范围
            // 注意：这里逻辑似乎是反的或者特定的？通常1表示占用，0表示空闲，但在SFS实现中freemap可能不同
            // 查看ucore代码，freemap中1表示占用，0表示空闲。
            // 这里代码：data[j/bits] |= (1<<(j%bits))
            // 似乎只在 next_ino 之后的位设置为1？
            // 或者是保留位？
            // 让我们仔细看: if (i + SFS_BLKBITS > next_ino)
            // next_ino是下一个可用inode索引。所以0 to next_ino-1 是已分配的。
            // 这里逻辑看起来像是在初始化某些位。
            if (i < next_ino) {
                start = next_ino - i;
            }
            if (i + SFS_BLKBITS > ninos) {
                end = ninos - i;
            }
            uint32_t *data = (uint32_t *)buffer;
            const uint32_t bits = sizeof(bits) * CHAR_BIT;
            for (j = start; j < end; j ++) {
                data[j / bits] |= (1 << (j % bits));
            }
        }
        write_block(sfs, buffer, sizeof(buffer), ino);
    }
    write_block(sfs, &(sfs->super), sizeof(sfs->super), SFS_BLKN_SUPER); // 写入超级块

    // 刷新哈希表中的缓存块和inode
    for (i = 0; i < HASH_LIST_SIZE; i ++) {
        struct cache_block *cb = sfs->blocks[i];
        while (cb != NULL) {
            flush_cache_block(sfs, cb);
            cb = cb->hash_next;
        }
        struct cache_inode *ci = sfs->inodes[i];
        while (ci != NULL) {
            flush_cache_inode(sfs, ci);
            ci = ci->hash_next;
        }
    }
}

// open_img - 打开镜像文件
struct sfs_fs *
open_img(const char *imgname) {
    const char *expect = ".img", *ext = imgname + strlen(imgname) - strlen(expect);
    if (ext <= imgname || strcmp(ext, expect) != 0) { // 检查后缀
        bug("invalid .img file name '%s'.\n", imgname);
    }
    int imgfd;
    if ((imgfd = open(imgname, O_WRONLY)) < 0) { // 只写模式打开
        bug("open '%s' failed.\n", imgname);
    }
    return create_sfs(imgfd);
}

#define open_bug(sfs, name, ...)                                                        \
    do {                                                                                \
        subpath_show(stderr, sfs, name);                                                \
        bug(__VA_ARGS__);                                                               \
    } while (0)

#define show_fullpath(sfs, name) subpath_show(stderr, sfs, name)

void open_dir(struct sfs_fs *sfs, struct cache_inode *current, struct cache_inode *parent);
void open_file(struct sfs_fs *sfs, struct cache_inode *file, const char *filename, int fd);
void open_link(struct sfs_fs *sfs, struct cache_inode *file, const char *filename);

#define SFS_BLK_NENTRY                          (SFS_BLKSIZE / sizeof(uint32_t)) // 每块入口数
#define SFS_L0_NBLKS                            SFS_NDIRECT // 直接索引块数
#define SFS_L1_NBLKS                            (SFS_BLK_NENTRY + SFS_L0_NBLKS) // 一级间接索引总块数
#define SFS_L2_NBLKS                            (SFS_BLK_NENTRY * SFS_BLK_NENTRY + SFS_L1_NBLKS) // 二级间接索引总块数
#define SFS_LN_NBLKS                            (SFS_MAX_FILE_SIZE / SFS_BLKSIZE) // 最大文件块数

// update_cache - 更新缓存块指针和inode号
static void
update_cache(struct sfs_fs *sfs, struct cache_block **cbp, uint32_t *inop) {
    uint32_t ino = *inop;
    struct cache_block *cb = *cbp;
    if (ino == 0) { // 如果尚未分配
        cb = alloc_cache_block(sfs, 0); // 分配新块
        ino = cb->ino; // 获取块号
    }
    else if (cb == NULL || cb->ino != ino) { // 如果缓存未命中或不匹配
        cb = search_cache_block(sfs, ino); // 查找缓存
        assert(cb != NULL && cb->ino == ino);
    }
    *cbp = cb, *inop = ino; // 更新指针和块号
}

// append_block - 向文件追加数据块
static void
append_block(struct sfs_fs *sfs, struct cache_inode *file, size_t size, uint32_t ino, const char *filename) {
    static_assert(SFS_LN_NBLKS <= SFS_L2_NBLKS);
    assert(size <= SFS_BLKSIZE);
    uint32_t nblks = file->nblks; // 当前块数
    struct inode *inode = &(file->inode);
    if (nblks >= SFS_LN_NBLKS) { // 检查文件大小限制
        open_bug(sfs, filename, "file is too big.\n");
    }
    if (nblks < SFS_L0_NBLKS) { // 直接索引
        inode->direct[nblks] = ino;
    }
    else if (nblks < SFS_L1_NBLKS) { // 一级间接索引
        nblks -= SFS_L0_NBLKS;
        update_cache(sfs, &(file->l1), &(inode->indirect)); // 获取/分配间接索引块
        uint32_t *data = file->l1->cache;
        data[nblks] = ino; // 写入块号
    }
    else if (nblks < SFS_L2_NBLKS) { // 二级间接索引
        nblks -= SFS_L1_NBLKS;
        update_cache(sfs, &(file->l2), &(inode->db_indirect)); // 获取/分配二级间接索引块
        uint32_t *data2 = file->l2->cache;
        update_cache(sfs, &(file->l1), &data2[nblks / SFS_BLK_NENTRY]); // 获取/分配一级间接索引块
        uint32_t *data1 = file->l1->cache;
        data1[nblks % SFS_BLK_NENTRY] = ino; // 写入块号
    }
    file->nblks ++; // 增加块数
    inode->size += size; // 增加文件大小
    inode->blocks ++; // 增加占用块数
}

// add_entry - 添加目录项
static void
add_entry(struct sfs_fs *sfs, struct cache_inode *current, struct cache_inode *file, const char *name) {
    static struct sfs_entry __entry, *entry = &__entry;
    assert(current->inode.type == SFS_TYPE_DIR && strlen(name) <= SFS_MAX_FNAME_LEN); // 断言是目录且文件名合法
    entry->ino = file->ino, strcpy(entry->name, name); // 填充目录项
    uint32_t entry_ino = sfs_alloc_ino(sfs); // 分配目录项所在的块号
    write_block(sfs, entry, sizeof(entry->name), entry_ino); // 写入目录项数据到新块
    append_block(sfs, current, sizeof(entry->name), entry_ino, name); // 将新块追加到父目录文件
    file->inode.nlinks ++; // 增加文件链接数
}

// add_dir - 添加目录
static void
add_dir(struct sfs_fs *sfs, struct cache_inode *parent, const char *dirname, int curfd, int fd, ino_t real) {
    assert(search_cache_inode(sfs, real) == NULL); // 确保未处理过
    struct cache_inode *current = alloc_cache_inode(sfs, real, 0, SFS_TYPE_DIR); // 分配目录inode
    safe_fchdir(fd), subpath_push(sfs, dirname); // 切换目录，压栈
    open_dir(sfs, current, parent); // 递归处理目录
    safe_fchdir(curfd), subpath_pop(sfs); // 恢复目录，弹栈
    add_entry(sfs, parent, current, dirname); // 添加到父目录
}

// add_file - 添加文件
static void
add_file(struct sfs_fs *sfs, struct cache_inode *current, const char *filename, int fd, ino_t real) {
    struct cache_inode *file;
    if ((file = search_cache_inode(sfs, real)) == NULL) { // 如果是新文件
        file = alloc_cache_inode(sfs, real, 0, SFS_TYPE_FILE); // 分配inode
        open_file(sfs, file, filename, fd); // 读取内容并写入
    }
    add_entry(sfs, current, file, filename); // 添加目录项
}

// add_link - 添加符号链接
static void
add_link(struct sfs_fs *sfs, struct cache_inode *current, const char *filename, ino_t real) {
    struct cache_inode *file = alloc_cache_inode(sfs, real, 0, SFS_TYPE_LINK); // 分配链接inode
    open_link(sfs, file, filename); // 读取链接内容
    add_entry(sfs, current, file, filename); // 添加目录项
}

// open_dir - 打开并读取目录
void
open_dir(struct sfs_fs *sfs, struct cache_inode *current, struct cache_inode *parent) {
    DIR *dir;
    if ((dir = opendir(".")) == NULL) { // 打开当前目录
        open_bug(sfs, NULL, "opendir failed.\n");
    }
    add_entry(sfs, current, current, "."); // 添加 .
    add_entry(sfs, current, parent, ".."); // 添加 ..
    struct dirent *direntp;
    while ((direntp = readdir(dir)) != NULL) { // 遍历目录项
        const char *name = direntp->d_name;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) { // 跳过 . 和 ..
            continue ;
        }
        if (name[0] == '.') { // 跳过隐藏文件
            continue ;
        }
        if (strlen(name) > SFS_MAX_FNAME_LEN) {
            open_bug(sfs, NULL, "file name is too long: %s\n", name);
        }
        struct stat *stat = safe_lstat(name); // 获取文件状态
        if (S_ISLNK(stat->st_mode)) { // 处理链接
            add_link(sfs, current, name, stat->st_ino);
        }
        else {
            int fd;
            if ((fd = open(name, O_RDONLY)) < 0) {
                open_bug(sfs, NULL, "open failed: %s\n", name);
            }
            if (S_ISDIR(stat->st_mode)) { // 处理子目录
                add_dir(sfs, current, name, dirfd(dir), fd, stat->st_ino);
            }
            else if (S_ISREG(stat->st_mode)) { // 处理普通文件
                add_file(sfs, current, name, fd, stat->st_ino);
            }
            else { // 忽略其他类型
                char mode = '?';
                if (S_ISFIFO(stat->st_mode)) mode = 'f';
                if (S_ISSOCK(stat->st_mode)) mode = 's';
                if (S_ISCHR(stat->st_mode)) mode = 'c';
                if (S_ISBLK(stat->st_mode)) mode = 'b';
                show_fullpath(sfs, NULL);
                warn("unsupported mode %07x (%c): file %s\n", stat->st_mode, mode, name);
            }
            close(fd);
        }
    }
    closedir(dir);
}

// open_file - 读取文件内容并写入SFS
void
open_file(struct sfs_fs *sfs, struct cache_inode *file, const char *filename, int fd) {
    static char buffer[SFS_BLKSIZE];
    ssize_t ret, last = SFS_BLKSIZE;
    while ((ret = read(fd, buffer, sizeof(buffer))) != 0) { // 读取宿主机文件
        assert(last == SFS_BLKSIZE);
        uint32_t ino = sfs_alloc_ino(sfs); // 分配数据块
        write_block(sfs, buffer, ret, ino); // 写入数据
        append_block(sfs, file, ret, ino, filename); // 记录到inode
        last = ret;
    }
    if (ret < 0) {
        open_bug(sfs, filename, "read file failed.\n");
    }
}

// open_link - 读取符号链接内容并写入SFS
void
open_link(struct sfs_fs *sfs, struct cache_inode *file, const char *filename) {
    static char buffer[SFS_BLKSIZE];
    uint32_t ino = sfs_alloc_ino(sfs); // 分配数据块
    ssize_t ret = readlink(filename, buffer, sizeof(buffer)); // 读取链接目标
    if (ret < 0 || ret == SFS_BLKSIZE) {
        open_bug(sfs, filename, "read link failed, %d", (int)ret);
    }
    write_block(sfs, buffer, ret, ino); // 写入数据
    append_block(sfs, file, ret, ino, filename); // 记录到inode
}

// create_img - 创建镜像
int
create_img(struct sfs_fs *sfs, const char *home) {
    int curfd, homefd;
    if ((curfd = open(".", O_RDONLY)) < 0) { // 保存当前目录fd
        bug("get current fd failed.\n");
    }
    if ((homefd = open(home, O_RDONLY | O_NOFOLLOW)) < 0) { // 打开根目录
        bug("open home directory '%s' failed.\n", home);
    }
    safe_fchdir(homefd); // 切换到根目录
    open_dir(sfs, sfs->root, sfs->root); // 开始递归构建
    safe_fchdir(curfd); // 恢复目录
    close(curfd), close(homefd);
    close_sfs(sfs); // 完成写入
    return 0;
}

static void
static_check(void) {
    static_assert(sizeof(off_t) == 8);
    static_assert(sizeof(ino_t) == 8);
    static_assert(SFS_MAX_NBLKS <= 0x80000000UL);
    static_assert(SFS_MAX_FILE_SIZE <= 0x80000000UL);
}

int
main(int argc, char **argv) {
    static_check();
    if (argc != 3) {
        bug("usage: <input *.img> <input dirname>\n");
    }
    const char *imgname = argv[1], *home = argv[2];
    if (create_img(open_img(imgname), home) != 0) {
        bug("create img failed.\n");
    }
    printf("create %s (%s) successfully.\n", imgname, home);
    return 0;
}

