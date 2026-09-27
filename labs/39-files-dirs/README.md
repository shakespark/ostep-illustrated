# 第 39 章实验：文件和目录的系统调用

五个小程序，对应原书第 39 章的五个主题。在 Linux / WSL2 上用 gcc 编译运行：

```bash
cd labs/39-files-dirs
make
./mycat          # 模拟 strace cat foo
./links          # 硬链接 vs 符号链接
./mystat         # 自己实现的 stat；目录的 link count
./fork-seek      # fork / dup / 两次 open 对偏移的影响
./atomic-write   # write 临时文件 + fsync + rename
make clean       # 删除可执行文件和残留的临时文件
```

每个程序都在当前目录下创建临时文件，运行结束前自己删除；万一中途被打断，`make clean` 会清理干净。

## 关于 strace

原书用 `strace` 观察程序发出的系统调用。本机没有安装 strace，需要 root 权限才能装：

```bash
sudo apt install strace
strace cat foo
strace -e trace=openat,read,write,close cat foo   # 新版 glibc 用 openat 代替 open
strace rm foo          # 看到 unlink("foo")
strace mv foo bar      # 看到 rename("foo", "bar")（或 renameat2）
strace -f ./fork-seek  # -f 连子进程一起跟踪
```

装不了 strace 时，`mycat` 自己执行 cat 的系统调用序列，并按 strace 的格式把每一步打印到标准错误。

## 各程序做什么、该观察什么

| 程序 | 做什么 | 观察什么 / 为什么 |
|---|---|---|
| `mycat [文件]` | open → 循环 read / write → close | 返回的 fd 是 **3**：0/1/2 已被标准输入、标准输出、标准错误占用。最后一次 read 返回 0，表示读到文件末尾。 |
| `links` | `link()`、`unlink()`、`symlink()`、`lstat()`、`readlink()` | 硬链接的 inode 号相同，Links 随 ln / rm 变化：1→2→3→2→1。符号链接有自己的 inode，大小等于路径名长度（"file" 4 字节，"alongerfilename" 15 字节）；删掉目标后 cat 失败，成为悬空引用。 |
| `mystat [路径...]` | 仿 `stat` 命令打印 `struct stat` | 6 字节的文件 Blocks=8（8×512B=4KB，按块分配）。不带参数时演示：目录的 link count = 2 + 子目录数，普通文件不影响它；rmdir 只能删除空目录。 |
| `fork-seek` | 复刻原书 Figure 39.2，并对比 dup、两次 open、dup2 | fork 后子进程 lseek 到 10，父进程看到的也是 10：父子共享同一个打开文件表项。dup 同样共享偏移；两次 open 则得到两个独立表项，偏移互不影响。dup2 把 fd 1 指向文件，就是 shell 的输出重定向。 |
| `atomic-write` | 写 `foo.txt.tmp` → fsync → close → rename → fsync 目录 | 任何一步崩溃，foo.txt 都是完整的旧版或完整的新版。rename 相对于崩溃是原子的；fsync 目录让"改名"这个目录修改本身也持久化。 |

注意：fsync 的效果无法在不断电的情况下直接"看到"，程序只能展示正确的调用顺序。它真正的意义要到崩溃时才显现，第 42 章（崩溃一致性）会细讲。

## 延伸练习（原书 Homework）

1. 给 `mystat` 加参数，观察目录的 link count 随子目录数量变化。
2. 写 `myls [-l] [目录]`：用 `opendir()` / `readdir()` / `closedir()`，`-l` 时对每一项再调用 `stat()`，打印属主、权限、大小。
3. 写 `mytail -n 文件`：先 `lseek()` 到接近文件末尾，读一块，再往回找换行符，而不是从头读整个文件。
4. 写一个递归遍历目录树的程序（类似 `find`）。
