#include "vfs.h"

static struct vfs_node nodes[VFS_NODE_MAX];
static int used;
static int root_id;
static int str_eq(const char *a,const char *b){while(*a&&*b&&*a==*b){a++;b++;}return *a==*b;}
static size_t str_len(const char *s){size_t n=0;while(s[n])n++;return n;}
static void str_copy(char *d,const char *s,size_t cap){size_t i=0;if(!cap)return;while(s[i]&&i+1<cap){d[i]=s[i];i++;}d[i]=0;}
static int alloc_node(const char *name,unsigned char type,int parent){int id;if(used>=VFS_NODE_MAX)return -1;id=used++;str_copy(nodes[id].name,name,VFS_NAME_MAX);nodes[id].type=type;nodes[id].parent=parent;nodes[id].first_child=-1;nodes[id].next_sibling=-1;nodes[id].size=0;return id;}
static int find_child(int parent,const char *name){int n=nodes[parent].first_child;while(n>=0){if(str_eq(nodes[n].name,name))return n;n=nodes[n].next_sibling;}return -1;}
static int add_child(int parent,const char *name,unsigned char type){int id=alloc_node(name,type,parent),n;if(id<0)return -1;if(nodes[parent].first_child<0)nodes[parent].first_child=id;else{n=nodes[parent].first_child;while(nodes[n].next_sibling>=0)n=nodes[n].next_sibling;nodes[n].next_sibling=id;}return id;}
void vfs_init(void){static const char *dirs[]={"bin","sbin","boot","dev","etc","home","lib","proc","root","tmp","usr","var"};size_t i;used=0;root_id=alloc_node("/",VFS_DIR,-1);for(i=0;i<sizeof(dirs)/sizeof(dirs[0]);i++)add_child(root_id,dirs[i],VFS_DIR);{int usr=find_child(root_id,"usr");add_child(usr,"bin",VFS_DIR);add_child(usr,"include",VFS_DIR);add_child(usr,"lib",VFS_DIR);add_child(usr,"share",VFS_DIR);} {int etc=find_child(root_id,"etc");int f=add_child(etc,"os-release",VFS_FILE);const char *s="NAME=alpOS\nVERSION=0.1-dev\n";vfs_write(f,s,str_len(s));}}
int vfs_root(void){return root_id;}
int vfs_lookup(int cwd,const char *path){int cur=cwd;size_t i=0;char part[VFS_NAME_MAX];if(!path||!*path)return cwd;if(path[0]=='/')cur=root_id;while(path[i]){size_t n=0;while(path[i]=='/')i++;if(!path[i])break;while(path[i]&&path[i]!='/'&&n+1<VFS_NAME_MAX)part[n++]=path[i++];part[n]=0;while(path[i]&&path[i]!='/')i++;if(str_eq(part,"."))continue;if(str_eq(part,"..")){if(nodes[cur].parent>=0)cur=nodes[cur].parent;continue;}if(nodes[cur].type!=VFS_DIR)return -1;cur=find_child(cur,part);if(cur<0)return -1;}return cur;}
static int create_path(int cwd,const char *path,unsigned char type){char parent[VFS_PATH_MAX],name[VFS_NAME_MAX];size_t len=str_len(path),i=len;int p;while(i>0&&path[i-1]!='/')i--;if(len-i==0||len-i>=VFS_NAME_MAX)return -1;{size_t j=0;while(i>0&&j+1<sizeof(parent)){parent[j]=path[j];j++;}parent[j]=0;}str_copy(name,path+i,VFS_NAME_MAX);p=vfs_lookup(cwd,parent[0]?parent:".");if(p<0||nodes[p].type!=VFS_DIR||find_child(p,name)>=0)return -1;return add_child(p,name,type);}
int vfs_mkdir(int cwd,const char *path){return create_path(cwd,path,VFS_DIR);}
int vfs_create(int cwd,const char *path){return create_path(cwd,path,VFS_FILE);}
int vfs_write(int node,const char *data,size_t len){size_t i;if(node<0||node>=used||nodes[node].type!=VFS_FILE||len>VFS_FILE_MAX)return -1;for(i=0;i<len;i++)nodes[node].data[i]=data[i];nodes[node].size=len;return (int)len;}
int vfs_read(int node,char *out,size_t cap){size_t i,n;if(node<0||node>=used||nodes[node].type!=VFS_FILE||!cap)return -1;n=nodes[node].size<cap-1?nodes[node].size:cap-1;for(i=0;i<n;i++)out[i]=nodes[node].data[i];out[n]=0;return (int)n;}
int vfs_child(int node){return node>=0&&node<used?nodes[node].first_child:-1;}
int vfs_next(int node){return node>=0&&node<used?nodes[node].next_sibling:-1;}
const struct vfs_node *vfs_get(int node){return node>=0&&node<used?&nodes[node]:0;}
