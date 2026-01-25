#ifndef XJOS_USER_BUILTIN_APPLETS_H
#define XJOS_USER_BUILTIN_APPLETS_H

int cmd_ls(int argc, char **argv, char **envp);
int cmd_cat(int argc, char **argv, char **envp);
int cmd_echo(int argc, char **argv, char **envp);
int cmd_env(int argc, char **argv, char **envp);
int cmd_pwd(int argc, char **argv, char **envp);
int cmd_clear(int argc, char **argv, char **envp);
int cmd_date(int argc, char **argv, char **envp);
int cmd_mkdir(int argc, char **argv, char **envp);
int cmd_rmdir(int argc, char **argv, char **envp);
int cmd_rm(int argc, char **argv, char **envp);
int cmd_mount(int argc, char **argv, char **envp);
int cmd_umount(int argc, char **argv, char **envp);
int cmd_mkfs(int argc, char **argv, char **envp);
int cmd_sh(int argc, char **argv, char **envp);

#endif /* XJOS_USER_BUILTIN_APPLETS_H */
