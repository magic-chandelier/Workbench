# Workbench

> A lightweight, text-based Linux operating environment for servers, SSH sessions, and low-resource systems.
>
> 面向 Linux 服务器、SSH 与低资源环境的轻量纯文字操作工作台。

**Current version: v0.15.0**

Workbench 不是新的 Linux 发行版，也不是某个现有管理面板的换皮。它运行在现有 Linux 之上，用一个轻量、纯终端、可鼠标/键盘操作的界面，把文件管理、Shell、Linux 指令和用户自定义任务集中到同一个工作台。

它的目标不是隐藏 Linux，而是让用户既能通过任务化界面完成操作，也能随时回到真实 Shell。

```text
Linux
  │
  └── Workbench
       ├── Workbench Files
       ├── Terminal
       ├── Command Sets / 指令集
       └── Settings
```

---

## Why Workbench?

Linux 服务器上的很多日常操作并不复杂，但命令分散、参数繁多，而且不同发行版存在差异。Workbench 尝试把这些操作整理成可浏览、可搜索、带参数和风险提示的任务，同时保留完整命令行能力。

Workbench 适合：

- SSH / VPS / 云服务器
- 无桌面环境 Linux
- 家庭服务器与个人实验环境
- Linux 初学者
- 需要快速查找和执行系统任务的用户
- CPU、内存和存储较有限的系统
- 希望键盘和鼠标都能操作的终端环境

---

# Main Interface

Workbench 首页保持非常简单：

```text
Workbench
──────────────────────────────
Workbench Files
Terminal
Command Sets / 指令集
Settings
Exit Workbench
```

所有核心功能都从这里进入。

---

# Workbench Files

Workbench Files 是 Workbench 自己实现的第一方终端文件管理器，不基于 `nnn`、`ranger`、`lf`、Yazi 或 Midnight Commander。

当前支持：

- 文件和目录浏览
- Locations / 位置
- Home 快速入口
- 多磁盘 / 多挂载点识别
- 网络挂载点显示
- 隐藏文件显示/隐藏
- 文件属性
- 内置文本查看
- 新建文件 / 目录
- 复制
- 移动
- 重命名
- 删除
- chmod 权限修改
- 鼠标单击 / 双击 / 滚轮 / 右键菜单
- 键盘快捷键

Workbench Files 尽可能直接使用 POSIX / Linux 文件系统 API，而不是通过 Shell 拼接 `cp`、`mv`、`rm` 等命令。

### Locations

Workbench 不模拟 Windows 的 `C:` / `D:` 盘符，而是按照 Linux 实际挂载模型显示存储位置：

```text
Locations
────────────────────────────────────────
Home        /home/user
System      /                 ext4
Data        /data             xfs
Backup      /backup           ext4
NAS         /mnt/nas          nfs4
```

数据主要来自：

```text
/proc/self/mountinfo
```

容量信息使用 `statvfs()` 获取。

`/proc`、`/sys`、`/dev`、`tmpfs`、cgroup 等系统伪文件系统默认隐藏，需要时可以手动显示。

---

# Workbench Terminal

Workbench 内置自己的 Terminal。打开 Terminal 时 **Workbench UI 不会退出**。

```text
Workbench Terminal
        │
        ├── Workbench UI
        ├── Terminal history
        ├── Keyboard / Mouse input
        ├── Command Sets
        │
        └── Linux PTY
             │
             └── Host Shell
```

Terminal 使用 Linux / POSIX PTY 接口运行宿主系统的 Shell：

- 优先使用 `$SHELL`
- `$SHELL` 不可用时回退 `/bin/sh`
- Workbench 不内嵌 Bash、Zsh、Fish 等 Shell 源码
- 返回首页不会结束当前 Shell
- 再次打开 Terminal 可以继续之前的会话

当前默认：

```text
Terminal log lines       1000
History scroll step        50
Clear on reopen           Off
```

这些参数都可以在 Settings 中修改。

Terminal 日志默认只保存在当前 Workbench 进程内，不自动写入磁盘。

Terminal 可以直接打开 Workbench 的 **Command Sets**，选择一条指令后将命令插入当前 Shell 输入行，而不是立即自动执行：

```text
Terminal
  ↓
Command Sets
  ↓
Linux Commands / Custom Set
  ↓
Action
  ↓
Insert into Shell
```

例如：

```bash
user@server:~$ ss -lntup_
```

用户仍然可以修改后再按 Enter。

> 当前 Terminal 是 Workbench 自己实现的基础终端解析器，并不是完整 xterm/vt100 仿真器。普通 Shell 和常规命令是主要目标；复杂全屏程序的兼容性会继续完善。

---

# Command Sets / 指令集

从 v0.15.0 开始，Workbench 不再把几百条 Linux Action 直接堆在首页，而是把 **Command Set / 指令集** 作为一级容器。

```text
Command Sets
──────────────────────────────
Linux Commands        [System] [Read-only]
My Server             [Custom]
Nginx                 [Custom]
Backup                [Custom]
+ New Command Set
```

## Linux Commands

Linux Commands 是 Workbench 内置的系统指令集：

```text
Linux Commands
├── System
├── Files
├── Text
├── Process
├── Network
├── Storage
├── Permission
├── Archive
└── User
```

系统预制指令集是 **Read-only**：

- 可以浏览
- 可以搜索
- 可以查看属性
- 可以执行
- 可以从 Terminal 插入
- **不能修改**
- **不能删除**
- **不能重命名**

当前 Linux Generic 包含 **374 个任务级 Action**。

这些 Action 是按“用户要完成的任务”划分，而不是简单的一条可执行程序对应一个菜单项。

例如 `find` 可以拆成：

- 按名称查找
- 按大小查找
- 按修改时间查找
- 查找空文件
- 查找符号链接

这比单纯列出 `find` 更适合任务化使用。

---

# Custom Command Sets

用户可以建立自己的指令集，例如：

```text
My Server
Nginx
Backup
Game Server
Deployment
```

自定义指令集支持：

- 新建
- 重命名
- 修改说明
- 删除
- 添加指令
- 修改指令
- 删除指令
- 搜索
- 属性
- 运行
- Terminal 插入

自定义指令可以保存：

```text
Name
Description
Command template
Keywords
Risk level
Arguments {1} ... {4}
```

例如：

```text
Name:
Reload Nginx

Command:
systemctl reload nginx

Risk:
Sensitive

Keywords:
nginx reload web
```

也可以使用参数：

```text
ssh {1}@{2}
```

Workbench 会继续使用现有的参数验证、shell quoting 和风险提示模型。

用户指令集默认保存在：

```text
~/.local/share/workbench/command-sets/
```

格式为 Workbench 自己的 `.wbc` 文件，不依赖 JSON、YAML 或 SQLite 运行库。

安全边界：

```text
目录权限        0700
.wbc 文件       0600
最多用户指令集   64
每个最多 Action 256
```

损坏的 `.wbc`、重复 ID 或试图覆盖系统 `linux` ID 的用户文件不会进入可执行集合。

---

# Linux Profiles

Workbench 的系统指令集采用：

```text
Linux Core
    +
Distribution Overlay
```

而不是为每个发行版复制一整套 Linux 指令。

## Linux Generic

默认模式永远是：

```text
Linux Generic
```

包含 **374 Actions**。

Workbench 可以读取 `/etc/os-release` 显示系统检测结果，但检测结果 **只作为参考，不会自动切换发行版模式**。

## CentOS Compatibility Profile

用户可以在 Settings 中手动启用 CentOS Profile。

启用后：

```text
Linux Core
    +
CentOS-specific Actions
    +
CentOS Overrides
    =
483 effective Actions
```

CentOS overlay 当前包含 **110 个 Action**，主要覆盖：

- DNF / RPM
- systemd / journal
- firewalld
- SELinux
- NetworkManager
- CentOS / systemd system operations

CentOS 专属 Action 会与 Linux 通用 Action 混合显示，而不是再创建一套重复界面。

**CentOS trademark notice:** CentOS is a trademark of Red Hat, Inc. Workbench uses the name only to describe compatibility. Workbench is not affiliated with or endorsed by Red Hat or the CentOS Project.

---

# Keyboard + Mouse

Workbench 自有 UI 的设计原则是：

> 核心功能同时提供 Keyboard Path 和 Mouse Path。

支持：

- Keyboard-only
- Mouse-only
- Keyboard + Mouse

覆盖：

- 首页
- Files
- Locations
- Terminal controls
- Command Sets
- Linux Actions
- Search
- Settings
- Properties
- Confirm dialogs
- Text input

鼠标依赖当前终端支持 SGR / xterm mouse reporting。

## Virtual Keyboard

Workbench 自带鼠标虚拟键盘，默认关闭，可以在 Settings 中开启。

主要用于实体键盘不可用时，通过鼠标输入：

- 文件名
- 路径
- 搜索词
- Action 参数
- 自定义指令集名称
- 自定义命令
- Terminal 输入

虚拟键盘目前不实现完整中文输入法。

## Shortcut Hints

快捷键提示默认开启，可以在 Settings 中关闭。

关闭提示不会禁用快捷键本身。

---

# Risk & Safety

Workbench 的 Action 使用三档风险等级：

- **Normal** — 只读或低风险操作
- **Sensitive** — 网络或状态修改操作
- **Privileged** — 高权限或明显改变系统的操作

高风险操作可以根据 Settings 中的确认策略要求用户再次确认。

Workbench 对递归删除等危险路径还有额外保护，例如拒绝：

```text
/
//
.
..
```

以及解析后实际指向根目录的危险目标。

Workbench Files 的删除也使用单独的确认机制。

---

# Lightweight by Design

Workbench 目前保持 C + libc / POSIX / Linux API 的轻量方向。

核心没有引入：

- Qt
- GTK
- Electron
- Node.js runtime
- Python runtime
- Java runtime
- ncurses
- readline
- libvterm

当前预编译 x86_64 二进制约 **500 KB** 级别，实际大小随编译器和版本变化。

设计目标是继续适合：

```text
512 MB RAM
低性能服务器 CPU
小型系统盘
SSH-only 环境
```

---

# External System Tools

Workbench 自己实现 UI、Files、Terminal、Command Sets、Profile、参数系统和安全校验。

部分 Linux Action 会调用宿主系统中已经安装的外部程序，例如：

| 类别 | 示例 |
|---|---|
| Base utilities | `ls`, `cp`, `mv`, `grep`, `sed`, `find` |
| Process | `ps`, `kill`, `top` |
| Network | `ip`, `ss`, `ping` |
| Archive | `tar`, `gzip`, `bzip2`, `xz`, `zip`, `cpio` |
| Remote | `ssh`, `scp`, `curl`, `wget` |
| CentOS | `dnf`, `rpm`, `systemctl`, `journalctl`, `firewall-cmd`, `nmcli` |

这些外部程序：

- 不属于 Workbench 源代码
- 默认不随 Workbench Release 分发
- 保持各自原有版权和许可证
- 是否存在取决于宿主 Linux

Workbench 不因为调用这些程序而宣称它们属于 Workbench。

更多边界说明：

- [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md)
- [`DEPENDENCY_POLICY.md`](DEPENDENCY_POLICY.md)

---

# Install

## Prebuilt x86_64 Binary

确认系统架构：

```bash
uname -m
```

如果输出：

```text
x86_64
```

安装：

```bash
chmod +x wb-v0.15.0-x86_64
sudo install -m 755 wb-v0.15.0-x86_64 /usr/local/bin/wb
```

之后在任意目录直接运行：

```bash
wb
```

自检：

```bash
wb --self-test
```

---

# Build From Source

解压：

```bash
tar -xzf workbench-v0.15.0-source.tar.gz
cd workbench-v0.15.0
```

编译：

```bash
make clean
make
```

运行：

```bash
./wb
```

完整测试：

```bash
make test
```

安装：

```bash
sudo install -m 755 wb /usr/local/bin/wb
```

以后直接：

```bash
wb
```

---

# Configuration & User Data

主要用户数据位置：

```text
~/.config/workbench/config
~/.local/share/workbench/command-sets/
```

Workbench Terminal 的短期日志默认只存在内存中，不自动写入文件。

---

# Project Structure

核心目录大致为：

```text
modules/
├── linux-core/
├── centos/
├── files-manager/
├── terminal/
└── command-sets/
```

职责：

```text
Workbench Core
├── TUI / input
├── Settings
├── Search / parameters / risk
├── Module & profile integration
└── Execution

Workbench Files
└── POSIX/Linux filesystem operations

Workbench Terminal
└── PTY + terminal state + short history

Command Sets
├── Linux system set [read-only]
└── User .wbc sets [editable]
```

---

# Development Direction

Workbench 仍处于持续开发阶段。

长期方向包括：

- 插件化 Command Sets
- 更多 Linux distribution profiles
- Docker / Podman
- Git
- Nginx / Web server tools
- Database tools
- System monitoring
- Terminal compatibility improvements
- 可安装 / 卸载的官方与授权插件

目标是逐步发展为一个适合低资源 Linux 的轻量、插件化文字操作环境，而不是堆成一个庞大的 Linux 发行版。

详细版本变化请查看 [`CHANGELOG.md`](CHANGELOG.md)。

---

# License & Usage

Workbench **不是 OSI 定义的开源项目**。公开源代码不代表授予任意修改、再分发或商业使用权。

当前项目定位为公开可查看源码、面向个人下载与使用的 Workbench 软件。除非另有明确书面授权：

- 个人可以下载和使用官方 Workbench
- 不向公司、企业、学校、机构或其他组织默认授予使用授权
- 不默认授予商业使用或商业化权利
- 不允许修改后重新发布
- 不允许二次创作后冒充原作者或官方版本
- 不允许未经授权重新分发源码或二进制
- 不允许将 Workbench 集成进收费产品、托管服务或商业服务
- 未明确授予的权利全部保留

未来某些 **插件、SDK、扩展格式或单独软件** 可以拥有独立的特别授权；只有对应项目明确写明时才适用，不自动扩展到 Workbench Core。

正式使用或再分发前，如仓库提供独立 `LICENSE`，以该文件及项目方明确书面授权为准；未明确授予的权利均视为保留。

---

# Copyright

Copyright © Workbench Project.

All rights reserved.
