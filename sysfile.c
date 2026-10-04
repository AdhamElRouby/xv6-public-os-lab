//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//

#include "types.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "mmu.h"
#include "proc.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "fcntl.h"

// Fetch the nth word-sized system call argument as a file descriptor
// and return both the descriptor and the corresponding struct file.
static int
argfd(int n, int *pfd, struct file **pf)
{
  int fd;
  struct file *f;

  if(argint(n, &fd) < 0)
    return -1;
  if(fd < 0 || fd >= NOFILE || (f=myproc()->ofile[fd]) == 0)
    return -1;
  if(pfd)
    *pfd = fd;
  if(pf)
    *pf = f;
  return 0;
}

// Allocate a file descriptor for the given file.
// Takes over file reference from caller on success.
static int
fdalloc(struct file *f)
{
  int fd;
  struct proc *curproc = myproc();

  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd] == 0){
      curproc->ofile[fd] = f;
      return fd;
    }
  }
  return -1;
}

int
sys_dup(void)
{
  struct file *f;
  int fd;

  if(argfd(0, 0, &f) < 0)
    return -1;
  if((fd=fdalloc(f)) < 0)
    return -1;
  filedup(f);
  return fd;
}

int
sys_read(void)
{
  struct file *f;
  int n;
  char *p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0)
    return -1;
  return fileread(f, p, n);
}

int
sys_write(void)
{
  struct file *f;
  int n;
  char *p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0)
    return -1;
  return filewrite(f, p, n);
}

int
sys_close(void)
{
  int fd;
  struct file *f;

  if(argfd(0, &fd, &f) < 0)
    return -1;
  myproc()->ofile[fd] = 0;
  fileclose(f);
  return 0;
}

int
sys_fstat(void)
{
  struct file *f;
  struct stat *st;

  if(argfd(0, 0, &f) < 0 || argptr(1, (void*)&st, sizeof(*st)) < 0)
    return -1;
  return filestat(f, st);
}

// Create the path new as a link to the same inode as old.
int
sys_link(void)
{
  char name[DIRSIZ], *new, *old;
  struct inode *dp, *ip;

  if(argstr(0, &old) < 0 || argstr(1, &new) < 0)
    return -1;

  begin_op();
  if((ip = namei(old)) == 0){
    end_op();
    return -1;
  }

  ilock(ip);
  if(ip->type == T_DIR){
    iunlockput(ip);
    end_op();
    return -1;
  }

  ip->nlink++;
  iupdate(ip);
  iunlock(ip);

  if((dp = nameiparent(new, name)) == 0)
    goto bad;
  ilock(dp);
  if(dp->dev != ip->dev || dirlink(dp, name, ip->inum) < 0){
    iunlockput(dp);
    goto bad;
  }
  iunlockput(dp);
  iput(ip);

  end_op();

  return 0;

bad:
  ilock(ip);
  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);
  end_op();
  return -1;
}

// Is the directory dp empty except for "." and ".." ?
static int
isdirempty(struct inode *dp)
{
  int off;
  struct dirent de;

  for(off=2*sizeof(de); off<dp->size; off+=sizeof(de)){
    if(readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
      panic("isdirempty: readi");
    if(de.inum != 0)
      return 0;
  }
  return 1;
}

//PAGEBREAK!
int
sys_unlink(void)
{
  struct inode *ip, *dp;
  struct dirent de;
  char name[DIRSIZ], *path;
  uint off;

  if(argstr(0, &path) < 0)
    return -1;

  begin_op();
  if((dp = nameiparent(path, name)) == 0){
    end_op();
    return -1;
  }

  ilock(dp);

  // Cannot unlink "." or "..".
  if(namecmp(name, ".") == 0 || namecmp(name, "..") == 0)
    goto bad;

  if((ip = dirlookup(dp, name, &off)) == 0)
    goto bad;
  ilock(ip);

  if(ip->nlink < 1)
    panic("unlink: nlink < 1");
  if(ip->type == T_DIR && !isdirempty(ip)){
    iunlockput(ip);
    goto bad;
  }

  memset(&de, 0, sizeof(de));
  if(writei(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
    panic("unlink: writei");
  if(ip->type == T_DIR){
    dp->nlink--;
    iupdate(dp);
  }
  iunlockput(dp);

  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);

  end_op();

  return 0;

bad:
  iunlockput(dp);
  end_op();
  return -1;
}

static struct inode*
create(char *path, short type, short major, short minor)
{
  struct inode *ip, *dp;
  char name[DIRSIZ];

  if((dp = nameiparent(path, name)) == 0)
    return 0;
  ilock(dp);

  if((ip = dirlookup(dp, name, 0)) != 0){
    iunlockput(dp);
    ilock(ip);
    if(type == T_FILE && ip->type == T_FILE)
      return ip;
    iunlockput(ip);
    return 0;
  }

  if((ip = ialloc(dp->dev, type)) == 0)
    panic("create: ialloc");

  ilock(ip);
  ip->major = major;
  ip->minor = minor;
  ip->nlink = 1;
  iupdate(ip);

  if(type == T_DIR){  // Create . and .. entries.
    dp->nlink++;  // for ".."
    iupdate(dp);
    // No ip->nlink++ for ".": avoid cyclic ref count.
    if(dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0)
      panic("create dots");
  }

  if(dirlink(dp, name, ip->inum) < 0)
    panic("create: dirlink");

  iunlockput(dp);

  return ip;
}

int
sys_open(void)
{
  char *path;
  int fd, omode;
  struct file *f;
  struct inode *ip;

  if(argstr(0, &path) < 0 || argint(1, &omode) < 0)
    return -1;

  begin_op();

  if(omode & O_CREATE){
    ip = create(path, T_FILE, 0, 0);
    if(ip == 0){
      end_op();
      return -1;
    }
  } else {
    if((ip = namei(path)) == 0){
      end_op();
      return -1;
    }
    ilock(ip);
    if(ip->type == T_DIR && omode != O_RDONLY){
      iunlockput(ip);
      end_op();
      return -1;
    }
  }

  if((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0){
    if(f)
      fileclose(f);
    iunlockput(ip);
    end_op();
    return -1;
  }
  iunlock(ip);
  end_op();

  f->type = FD_INODE;
  f->ip = ip;
  f->off = 0;
  f->readable = !(omode & O_WRONLY);
  f->writable = (omode & O_WRONLY) || (omode & O_RDWR);
  return fd;
}

int
sys_mkdir(void)
{
  char *path;
  struct inode *ip;

  begin_op();
  if(argstr(0, &path) < 0 || (ip = create(path, T_DIR, 0, 0)) == 0){
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

int
sys_mknod(void)
{
  struct inode *ip;
  char *path;
  int major, minor;

  begin_op();
  if((argstr(0, &path)) < 0 ||
     argint(1, &major) < 0 ||
     argint(2, &minor) < 0 ||
     (ip = create(path, T_DEV, major, minor)) == 0){
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

int
sys_chdir(void)
{
  char *path;
  struct inode *ip;
  struct proc *curproc = myproc();
  
  begin_op();
  if(argstr(0, &path) < 0 || (ip = namei(path)) == 0){
    end_op();
    return -1;
  }
  ilock(ip);
  if(ip->type != T_DIR){
    iunlockput(ip);
    end_op();
    return -1;
  }
  iunlock(ip);
  iput(curproc->cwd);
  end_op();
  curproc->cwd = ip;
  return 0;
}

int
sys_exec(void)
{
  char *path, *argv[MAXARG];
  int i;
  uint uargv, uarg;

  if(argstr(0, &path) < 0 || argint(1, (int*)&uargv) < 0){
    return -1;
  }
  memset(argv, 0, sizeof(argv));
  for(i=0;; i++){
    if(i >= NELEM(argv))
      return -1;
    if(fetchint(uargv+4*i, (int*)&uarg) < 0)
      return -1;
    if(uarg == 0){
      argv[i] = 0;
      break;
    }
    if(fetchstr(uarg, &argv[i]) < 0)
      return -1;
  }
  return exec(path, argv);
}

int
sys_pipe(void)
{
  int *fd;
  struct file *rf, *wf;
  int fd0, fd1;

  if(argptr(0, (void*)&fd, 2*sizeof(fd[0])) < 0)
    return -1;
  if(pipealloc(&rf, &wf) < 0)
    return -1;
  fd0 = -1;
  if((fd0 = fdalloc(rf)) < 0 || (fd1 = fdalloc(wf)) < 0){
    if(fd0 >= 0)
      myproc()->ofile[fd0] = 0;
    fileclose(rf);
    fileclose(wf);
    return -1;
  }
  fd[0] = fd0;
  fd[1] = fd1;
  return 0;
}

static int
get_curr_dir_name(struct inode* cwd, char* buf, int size) {
  if(cwd->dev == ROOTDEV && cwd->inum == ROOTINO) {
    safestrcpy(buf, "/", size);
    return 0;
  }

  ilock(cwd);
  struct inode *parent = dirlookup(cwd, "..", 0);
  iunlock(cwd);
  if(parent == 0)
    return -1;

  if(get_curr_dir_name(parent, buf, size) < 0){
    iput(parent);
    return -1;
  }

  ilock(parent);
  struct dirent de;
  int found = 0;
  for(uint off = 0; off < parent->size; off += sizeof(de)) {
    if(readi(parent, (char*)&de, off, sizeof(de)) != sizeof(de))
      panic("getcwd readi");

    if(de.inum == cwd->inum){
      // This entry points to current.
      // de.name is its directory name.
      int len = strlen(buf);
      char name[DIRSIZ + 1];
      memmove(name, de.name, DIRSIZ);
      name[DIRSIZ] = 0; // Null-terminate the name to avoid buffer overflows.
      int backslash_needed = (len > 1 ? 1 : 0);
      int extra = strlen(name) + backslash_needed;      
      if(len + extra + 1 > size) {
        iunlock(parent);
        iput(parent);
        return -1;
      }
      if(backslash_needed)
        safestrcpy(buf + len, "/", size - len);
      safestrcpy(buf + len + backslash_needed, name,
                 size - len - backslash_needed);
      found = 1;
      break;
    }
  }
  iunlock(parent);
  iput(parent);
  return found ? 0 : -1;
}

int
sys_getcwd(void)
{
  char *buf;
  int size;

  if (argint(1, &size) < 0 || size <= 0 ||
      argptr(0, &buf, size) < 0) {
    return -1;
  }

  struct proc *curproc = myproc();
  struct inode *cwd = curproc->cwd;

  return get_curr_dir_name(cwd, buf, size);
}

int
sys_rename(void)
{
  char *oldpath, *newpath;
  char old_name[DIRSIZ], new_name[DIRSIZ];
  struct inode *old_parent = 0, *old_inode = 0, *new_parent = 0, *existing = 0;
  struct inode *ip, *next;
  struct dirent de;
  uint off;
  int old_isdir, ex_isdir = 0, ret = -1;

  if(argstr(0, &oldpath) < 0 || argstr(1, &newpath) < 0)
    return -1;
  if(strlen(oldpath) == strlen(newpath) &&
     memcmp(oldpath, newpath, strlen(oldpath) + 1) == 0)
    return 0;

  begin_op();

  old_parent = nameiparent(oldpath, old_name);
  if(old_parent == 0)
    goto done;

  old_inode = namei(oldpath);
  if(old_inode == 0)
    goto done;

  new_parent = nameiparent(newpath, new_name);
  if(new_parent == 0)
    goto done;

  // Never rename "." or ".." (either side)
  if(strncmp(old_name, ".", DIRSIZ) == 0 || strncmp(old_name, "..", DIRSIZ) == 0 ||
     strncmp(new_name, ".", DIRSIZ) == 0 || strncmp(new_name, "..", DIRSIZ) == 0)
    goto done;

  ilock(old_inode);
  old_isdir = (old_inode->type == T_DIR);
  iunlock(old_inode);

  // A directory must not be moved into its own subtree:
  // walk up from new_parent via ".." and make sure we never meet old_inode.
  if(old_isdir) {
    ip = idup(new_parent);
    while(1) {
      if(ip->inum == old_inode->inum) {
        iput(ip);
        goto done;
      }
      if(ip->inum == ROOTINO) {
        iput(ip);
        break;
      }
      ilock(ip);
      next = dirlookup(ip, "..", 0);
      iunlockput(ip);
      if(next == 0)
        goto done;
      ip = next;
    }
  }

  existing = namei(newpath);
  if(existing != 0) {
    // Same inode (e.g. "a" -> "./a" or hard links): nothing to do
    if(existing->inum == old_inode->inum) {
      ret = 0;
      goto done;
    }

    ilock(existing);
    ex_isdir = (existing->type == T_DIR);
    // Type must match; a directory victim must be empty
    if(ex_isdir != old_isdir || (ex_isdir && !isdirempty(existing))) {
      iunlock(existing);
      goto done;
    }
    iunlock(existing);

    // Overwrite the destination entry in place (cannot fail after this point)
    ilock(new_parent);
    for(off = 0; off < new_parent->size; off += sizeof(de)) {
      if(readi(new_parent, (char*)&de, off, sizeof(de)) != sizeof(de))
        panic("rename readi");
      if(de.inum != 0 && strncmp(de.name, new_name, DIRSIZ) == 0) {
        de.inum = old_inode->inum;
        if(writei(new_parent, (char*)&de, off, sizeof(de)) != sizeof(de))
          panic("rename writei");
        break;
      }
    }
    iunlock(new_parent);

    // The victim lost its link; iput() below frees it if nlink hits 0
    ilock(existing);
    existing->nlink--;
    iupdate(existing);
    iunlock(existing);

    // A removed directory's ".." no longer points at new_parent
    if(ex_isdir) {
      ilock(new_parent);
      new_parent->nlink--;
      iupdate(new_parent);
      iunlock(new_parent);
    }
  } else {
    // Nothing destroyed yet, so failing here is safe
    ilock(new_parent);
    int link_result = dirlink(new_parent, new_name, old_inode->inum);
    iunlock(new_parent);
    if(link_result < 0)
      goto done;
  }

  // Remove the old entry
  ilock(old_parent);
  for(off = 0; off < old_parent->size; off += sizeof(de)) {
    if(readi(old_parent, (char*)&de, off, sizeof(de)) != sizeof(de))
      panic("rename readi");
    if(de.inum == old_inode->inum &&
       strncmp(de.name, old_name, DIRSIZ) == 0) {
      memset(&de, 0, sizeof(de));
      if(writei(old_parent, (char*)&de, off, sizeof(de)) != sizeof(de))
        panic("rename writei");
      break;
    }
  }
  iunlock(old_parent);

  // Moving a directory to a different parent: fix ".." and parent link counts
  if(old_isdir && old_parent->inum != new_parent->inum) {
    ilock(old_inode);
    for(off = 0; off < old_inode->size; off += sizeof(de)) {
      if(readi(old_inode, (char*)&de, off, sizeof(de)) != sizeof(de))
        panic("rename readi");
      if(strncmp(de.name, "..", DIRSIZ) == 0) {
        de.inum = new_parent->inum;
        if(writei(old_inode, (char*)&de, off, sizeof(de)) != sizeof(de))
          panic("rename writei");
        break;
      }
    }
    iunlock(old_inode);

    ilock(old_parent);
    old_parent->nlink--;
    iupdate(old_parent);
    iunlock(old_parent);

    ilock(new_parent);
    new_parent->nlink++;
    iupdate(new_parent);
    iunlock(new_parent);
  }
  ret = 0;

done:
  if(existing)   iput(existing);
  if(new_parent) iput(new_parent);
  if(old_inode)  iput(old_inode);
  if(old_parent) iput(old_parent);
  end_op();
  return ret;
}