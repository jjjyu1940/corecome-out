#ifndef ALPOS_VFS_H
#define ALPOS_VFS_H
#include <stddef.h>
#define VFS_NAME_MAX 32
#define VFS_PATH_MAX 128
#define VFS_FILE_MAX 256
#define VFS_NODE_MAX 96
#define VFS_DIR 1
#define VFS_FILE 2
struct vfs_node {
    char name[VFS_NAME_MAX];
    unsigned char type;
    int parent;
    int first_child;
    int next_sibling;
    size_t size;
    char data[VFS_FILE_MAX];
};
void vfs_init(void);
int vfs_root(void);
int vfs_lookup(int cwd, const char *path);
int vfs_mkdir(int cwd, const char *path);
int vfs_create(int cwd, const char *path);
int vfs_write(int node, const char *data, size_t len);
int vfs_read(int node, char *out, size_t cap);
int vfs_child(int node);
int vfs_next(int node);
const struct vfs_node *vfs_get(int node);
#endif
