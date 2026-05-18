#include <xjos/string.h>
#include <xjos/types.h>
#include <xjos/cpu.h>
#include <xjos/printk.h>
#include <xjos/debug.h>
#include <xjos/errno.h>
#include <xjos/net.h>
#include <xjos/string.h>
#include <fs/stat.h>
#include <fs/fs.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

extern int sys_stat(char *filename, stat_t *statbuf);
extern int sys_chdir(char *pathname);
extern char *sys_getcwd(char *buf, size_t size);
extern fd_t sys_open(char *filename, int flags, int mode);
extern void sys_close(fd_t fd);
extern int sys_read(fd_t fd, char *buf, int count);
extern int sys_readdir(fd_t fd, dirent_t *dir, u32 count);
extern int sys_write(unsigned int fd, char *buf, int count);
extern int sys_mkdir(char *pathname, int mode);
extern int sys_rmdir(char *pathname);
extern int sys_link(char *oldname, char *newname);
extern int sys_unlink(char *filename);

static err_t fs_expect(bool cond, const char *msg) {
    if (!cond) {
        LOGK("fs test failed: %s\n", msg);
        return -ERROR;
    }
    return EOK;
}

static err_t fs_expect_dir_contains(const char *path, const char *name) {
    fd_t fd = sys_open((char *)path, O_RDONLY, 0);
    if (fd < 0) {
        LOGK("fs test open failed: %s ret=%d\n", path, fd);
        return fd;
    }

    dirent_t entry;
    err_t ret = -ENOENT;
    while (true) {
        int len = sys_readdir(fd, &entry, 1);
        if (len <= 0)
            break;
        if (!entry.nr)
            continue;
        if (!strcmp(entry.name, name)) {
            ret = EOK;
            break;
        }
    }

    sys_close(fd);
    if (ret < 0)
        LOGK("fs test missing entry: dir=%s name=%s\n", path, name);
    return ret;
}

static err_t fs_expect_dir_missing(const char *path, const char *name) {
    fd_t fd = sys_open((char *)path, O_RDONLY, 0);
    if (fd < 0) {
        LOGK("fs test open failed: %s ret=%d\n", path, fd);
        return fd;
    }

    dirent_t entry;
    while (true) {
        int len = sys_readdir(fd, &entry, 1);
        if (len <= 0)
            break;
        if (!entry.nr)
            continue;
        if (!strcmp(entry.name, name)) {
            sys_close(fd);
            LOGK("fs test unexpected entry: dir=%s name=%s\n", path, name);
            return -EEXIST;
        }
    }

    sys_close(fd);
    return EOK;
}

static err_t fs_write_file(const char *path, char *data, int len) {
    fd_t fd = sys_open((char *)path, O_CREAT | O_TRUNC | O_RDWR, 0644);
    if (fd < 0) {
        LOGK("fs test create failed: %s ret=%d\n", path, fd);
        return fd;
    }

    int written = sys_write(fd, data, len);
    sys_close(fd);
    if (written != len) {
        LOGK("fs test short write: %s want=%d got=%d\n", path, len, written);
        return written < 0 ? written : -EIO;
    }
    return EOK;
}

static err_t fs_expect_file_content(const char *path, const char *expect) {
    char buf[128];
    int len = strlen(expect);

    fd_t fd = sys_open((char *)path, O_RDONLY, 0);
    if (fd < 0) {
        LOGK("fs test open read failed: %s ret=%d\n", path, fd);
        return fd;
    }

    int nr = sys_read(fd, buf, sizeof(buf) - 1);
    sys_close(fd);
    if (nr < 0) {
        LOGK("fs test read failed: %s ret=%d\n", path, nr);
        return nr;
    }

    buf[nr] = EOS;
    return fs_expect(nr == len && !strcmp(buf, expect), "file content mismatch");
}

static err_t fs_selftest(void) {
    char cwd[128];
    stat_t st;
    err_t ret;
    static char data[] = "fs regression payload";

    ret = fs_expect(sys_stat("/", &st) == EOK && ISDIR(st.mode), "root stat");
    if (ret < 0) return ret;

    ret = fs_expect(sys_stat("/dev", &st) == EOK && ISDIR(st.mode), "/dev stat");
    if (ret < 0) return ret;

    ret = fs_expect(sys_stat("/mnt", &st) == EOK && ISDIR(st.mode), "/mnt stat");
    if (ret < 0) return ret;

    ret = fs_expect(sys_stat("/hello.txt", &st) == EOK && ISFILE(st.mode), "/hello.txt stat");
    if (ret < 0) return ret;

    ret = fs_expect(fs_expect_dir_contains("/", "bin") == EOK, "root contains bin");
    if (ret < 0) return ret;

    ret = fs_expect(fs_expect_dir_contains("/", "dev") == EOK, "root contains dev");
    if (ret < 0) return ret;

    ret = fs_expect(fs_expect_dir_contains("/dev", "console") == EOK, "/dev contains console");
    if (ret < 0) return ret;

    ret = fs_expect(fs_expect_dir_contains("/dev", "stdin") == EOK, "/dev contains stdin");
    if (ret < 0) return ret;

    ret = fs_expect(sys_chdir("/no-such-dir") == -ENOENT, "chdir missing dir");
    if (ret < 0) return ret;

    ret = sys_chdir("/dev");
    if (ret < 0) {
        LOGK("fs test chdir /dev failed: %d\n", ret);
        return ret;
    }

    ret = fs_expect(!strcmp(sys_getcwd(cwd, sizeof(cwd)), "/dev"), "getcwd after /dev");
    if (ret < 0) return ret;

    ret = sys_chdir("..");
    if (ret < 0) {
        LOGK("fs test chdir .. failed: %d\n", ret);
        return ret;
    }

    ret = fs_expect(!strcmp(sys_getcwd(cwd, sizeof(cwd)), "/"), "getcwd after /dev/..");
    if (ret < 0) return ret;

    ret = sys_chdir("/mnt");
    if (ret < 0) {
        LOGK("fs test chdir /mnt failed: %d\n", ret);
        return ret;
    }

    ret = fs_expect(!strcmp(sys_getcwd(cwd, sizeof(cwd)), "/mnt"), "getcwd after /mnt");
    if (ret < 0) return ret;

    ret = sys_chdir(".");
    if (ret < 0) {
        LOGK("fs test chdir . failed: %d\n", ret);
        return ret;
    }

    ret = fs_expect(!strcmp(sys_getcwd(cwd, sizeof(cwd)), "/mnt"), "getcwd after /mnt/.");
    if (ret < 0) return ret;

    ret = sys_chdir("..");
    if (ret < 0) {
        LOGK("fs test chdir /mnt/.. failed: %d\n", ret);
        return ret;
    }

    ret = fs_expect(!strcmp(sys_getcwd(cwd, sizeof(cwd)), "/"), "getcwd after /mnt/..");
    if (ret < 0) return ret;

    ret = sys_chdir("/mnt");
    if (ret < 0) {
        LOGK("fs test chdir /mnt before rw tests failed: %d\n", ret);
        return ret;
    }

    (void)sys_unlink("t-link");
    (void)sys_unlink("t-file");
    (void)sys_rmdir("t-dir");

    ret = sys_mkdir("t-dir", 0755);
    ret = fs_expect(ret == EOK, "mkdir /mnt/t-dir");
    if (ret < 0) return ret;

    ret = fs_expect(fs_expect_dir_contains("/mnt", "t-dir") == EOK, "/mnt contains t-dir");
    if (ret < 0) return ret;

    ret = sys_chdir("t-dir");
    if (ret < 0) {
        LOGK("fs test chdir t-dir failed: %d\n", ret);
        return ret;
    }

    ret = fs_expect(!strcmp(sys_getcwd(cwd, sizeof(cwd)), "/mnt/t-dir"), "getcwd after /mnt/t-dir");
    if (ret < 0) return ret;

    ret = fs_write_file("/mnt/t-dir/note", data, strlen(data));
    if (ret < 0) return ret;

    ret = fs_expect_file_content("/mnt/t-dir/./note", data);
    if (ret < 0) return ret;

    ret = fs_expect_file_content("/mnt/t-dir/../t-dir/note", data);
    if (ret < 0) return ret;

    ret = fs_expect_file_content("/mnt/./t-dir/../t-dir/./note", data);
    if (ret < 0) return ret;

    ret = sys_chdir("..");
    if (ret < 0) {
        LOGK("fs test chdir t-dir/.. failed: %d\n", ret);
        return ret;
    }

    ret = fs_expect(!strcmp(sys_getcwd(cwd, sizeof(cwd)), "/mnt"), "getcwd after /mnt/t-dir/..");
    if (ret < 0) return ret;

    ret = fs_expect(sys_rmdir("/mnt/t-dir") == -ENOTEMPTY, "rmdir non-empty dir");
    if (ret < 0) return ret;

    ret = fs_expect(sys_unlink("/mnt/t-dir") == -EPERM, "unlink directory denied");
    if (ret < 0) return ret;

    ret = sys_unlink("/mnt/t-dir/note");
    ret = fs_expect(ret == EOK, "unlink /mnt/t-dir/note");
    if (ret < 0) return ret;

    ret = fs_expect(fs_expect_dir_missing("/mnt/t-dir", "note") == EOK, "/mnt/t-dir missing note");
    if (ret < 0) return ret;

    ret = fs_expect(sys_open("/mnt/t-dir/../no-such", O_RDONLY, 0) == -ENOENT, "open relative missing through ..");
    if (ret < 0) return ret;

    (void)sys_unlink("/mnt/a/b/deep");
    (void)sys_rmdir("/mnt/a/b");
    (void)sys_rmdir("/mnt/a");

    ret = sys_mkdir("/mnt/a", 0755);
    ret = fs_expect(ret == EOK, "mkdir /mnt/a");
    if (ret < 0) return ret;

    ret = sys_mkdir("/mnt/a/b", 0755);
    ret = fs_expect(ret == EOK, "mkdir /mnt/a/b");
    if (ret < 0) return ret;

    ret = fs_expect(fs_expect_dir_contains("/mnt", "a") == EOK, "/mnt contains a");
    if (ret < 0) return ret;

    ret = fs_expect(fs_expect_dir_contains("/mnt/a", "b") == EOK, "/mnt/a contains b");
    if (ret < 0) return ret;

    ret = sys_chdir("/mnt/a/./b/..");
    if (ret < 0) {
        LOGK("fs test chdir /mnt/a/./b/.. failed: %d\n", ret);
        return ret;
    }

    ret = fs_expect(!strcmp(sys_getcwd(cwd, sizeof(cwd)), "/mnt/a"), "getcwd after /mnt/a/./b/..");
    if (ret < 0) return ret;

    ret = fs_write_file("/mnt/a/b/deep", data, strlen(data));
    if (ret < 0) return ret;

    ret = fs_expect_file_content("/mnt/a/b/deep", data);
    if (ret < 0) return ret;

    ret = fs_expect_file_content("/mnt/a/./b/../b/deep", data);
    if (ret < 0) return ret;

    ret = fs_expect_file_content("/mnt/a/b/../../a/b/deep", data);
    if (ret < 0) return ret;

    ret = sys_chdir("/mnt");
    if (ret < 0) {
        LOGK("fs test chdir /mnt before nested cleanup failed: %d\n", ret);
        return ret;
    }

    ret = sys_unlink("/mnt/a/b/deep");
    ret = fs_expect(ret == EOK, "unlink /mnt/a/b/deep");
    if (ret < 0) return ret;

    ret = fs_expect(fs_expect_dir_missing("/mnt/a/b", "deep") == EOK, "/mnt/a/b missing deep");
    if (ret < 0) return ret;

    ret = sys_rmdir("/mnt/a/b");
    ret = fs_expect(ret == EOK, "rmdir /mnt/a/b");
    if (ret < 0) return ret;

    ret = sys_rmdir("/mnt/a");
    ret = fs_expect(ret == EOK, "rmdir /mnt/a");
    if (ret < 0) return ret;

    ret = fs_expect(fs_expect_dir_missing("/mnt", "a") == EOK, "/mnt missing a");
    if (ret < 0) return ret;

    ret = fs_write_file("/mnt/t-file", data, strlen(data));
    if (ret < 0) return ret;

    ret = fs_expect(sys_stat("/mnt/t-file", &st) == EOK && ISFILE(st.mode), "stat /mnt/t-file");
    if (ret < 0) return ret;

    ret = fs_expect(st.size == strlen(data), "t-file size");
    if (ret < 0) return ret;

    ret = fs_expect_file_content("/mnt/t-file", data);
    if (ret < 0) return ret;

    ret = sys_link("/mnt/t-file", "/mnt/t-link");
    ret = fs_expect(ret == EOK, "link /mnt/t-file -> /mnt/t-link");
    if (ret < 0) return ret;

    ret = fs_expect(sys_stat("/mnt/t-link", &st) == EOK && ISFILE(st.mode), "stat /mnt/t-link");
    if (ret < 0) return ret;

    ret = fs_expect(st.nlinks >= 2, "t-link nlinks");
    if (ret < 0) return ret;

    ret = fs_expect_file_content("/mnt/t-link", data);
    if (ret < 0) return ret;

    ret = sys_unlink("/mnt/t-link");
    ret = fs_expect(ret == EOK, "unlink /mnt/t-link");
    if (ret < 0) return ret;

    ret = fs_expect(fs_expect_dir_missing("/mnt", "t-link") == EOK, "/mnt missing t-link");
    if (ret < 0) return ret;

    ret = sys_unlink("/mnt/t-file");
    ret = fs_expect(ret == EOK, "unlink /mnt/t-file");
    if (ret < 0) return ret;

    ret = fs_expect(fs_expect_dir_missing("/mnt", "t-file") == EOK, "/mnt missing t-file");
    if (ret < 0) return ret;

    ret = sys_rmdir("/mnt/t-dir");
    ret = fs_expect(ret == EOK, "rmdir /mnt/t-dir");
    if (ret < 0) return ret;

    ret = fs_expect(fs_expect_dir_missing("/mnt", "t-dir") == EOK, "/mnt missing t-dir");
    if (ret < 0) return ret;

    ret = sys_chdir("/");
    if (ret < 0) {
        LOGK("fs test restore / failed: %d\n", ret);
        return ret;
    }

    ret = fs_expect(!strcmp(sys_getcwd(cwd, sizeof(cwd)), "/"), "getcwd after final restore /");
    if (ret < 0) return ret;

    ret = fs_expect(fs_expect_dir_contains(".", "bin") == EOK, "cwd root contains bin");
    if (ret < 0) return ret;

    return EOK;
}

err_t sys_test() {
    err_t ret = fs_selftest();
    if (ret < 0)
        return ret;

    LOGK("fs selftest passed\n");
    return EOK;
}
