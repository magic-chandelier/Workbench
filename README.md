# Workbench

> A lightweight, text-based Linux operating environment for servers, SSH sessions, and low-resource systems.
>
> 面向 Linux 服务器、SSH 与低资源环境的轻量纯文字操作工作台。

**Current version: v0.16.1**

Workbench 不是新的 Linux 发行版，也不是现有管理面板的换皮。它运行在现有 Linux 之上，以一个轻量、纯终端、同时支持键盘与鼠标的界面，把文件管理、真实 Shell、Linux 指令集、用户任务和插件能力集中到同一个工作台。

它的目标不是隐藏 Linux，而是让用户既能通过任务化界面完成常见操作，也能随时回到真实 Shell。

```text
Linux
  │
  └── Workbench
       ├── Workbench Files
       ├── Terminal
       ├── Command Sets / 指令集
       ├── Applications / 应用（由插件动态提供）
       ├── Plugin / Extension System
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
- 希望通过插件扩展文件操作、预览和工具能力的环境

---

# Main Interface

Workbench 首页保持简单：

```text
Workbench
──────────────────────────────
Workbench Files
Terminal
Command Sets / 指令集
Applications / 应用    ← 仅在有应用型插件时显示
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
- 隐藏文件显示 / 隐藏
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
- 插件贡献的 Files 右键操作
- 插件贡献的只读文件预览 / 虚拟目录浏览

Workbench Files 尽可能直接使用 POSIX / Linux 文件系统 API，而不是通过 Shell 拼接 `cp`、`mv`、`rm` 等命令。

## Plugin File Actions

从 v0.16.1 起，兼容 Plugin API 2+ 的插件可以向 Workbench Files 右键菜单贡献通用 File Action。

Core 本身不识别 7-Zip、图片编辑器、校验工具或其他具体产品。插件自己声明：

```text
Action ID
显示名称
目标类型：file / directory / any
文件扩展名过滤
执行入口
```

执行时，Workbench 将当前选中路径作为独立 argv 参数传给插件，不通过 Shell 拼接。

协议：

```text
entry --workbench-file-action <action-id> -- <absolute-selected-path>
```

在真正执行前，Workbench 会重新检查插件状态、Action descriptor、执行入口和选中目标，避免禁用、删除或被修改后的旧 Registry 项继续运行。

## Files Preview Providers

Plugin API 3 增加通用的只读 Files Preview Provider。

插件负责提供虚拟目录内容和按需 materialize 单个文件；目录导航、键盘 / 鼠标操作、属性页、隐藏文件过滤以及内置文本查看仍由 Workbench Files 提供。

例如归档类插件可以让用户：

```text
archive.7z
  ↓ Enter
Archive Preview (read-only)
  ├── folder/
  │    └── file.txt
  └── image.png
```

Preview 是明确的**只读虚拟 Files**。当前不会把普通文件系统的以下操作伪装成归档内操作：

- Rename
- Move
- Delete
- chmod
- 原地写回归档

这些操作需要未来单独设计事务式 Archive Edit / Virtual Files Write API。

Workbench 对 Preview Provider 输出进行独立安全校验，包括拒绝绝对路径、`.` / `..` 路径组件、控制字符、非法编码和超量输出。

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
Linux Commands / Custom / Plugin Set
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

Workbench 使用 **Command Set / 指令集** 作为任务容器：

```text
Command Sets
──────────────────────────────
Linux Commands        [System] [Read-only]
My Server             [Custom]
Nginx                 [Custom]
7-Zip Commands        [Plugin] [Read-only]
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
- 不能修改
- 不能删除
- 不能重命名

当前 Linux Generic 包含 **374 个任务级 Action**。

这些 Action 按“用户要完成的任务”划分，而不是简单地一条可执行程序对应一个菜单项。

例如 `find` 可以拆成：

- 按名称查找
- 按大小查找
- 按修改时间查找
- 查找空文件
- 查找符号链接

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

# Plugin & Extension System

Workbench v0.16.1 正式提供通用插件系统。

插件目录：

```text
/usr/local/share/workbench/plugins/   System plugins
~/.local/share/workbench/plugins/     User plugins
```

每个插件是一个独立目录，并使用 `plugin.wbp` 描述自身。目录名必须与插件 ID 一致。

当前宿主：

```text
Plugin API     3
Extension API  3
Compatible     API 1 .. 3
```

能力按 API 代际累积：

```text
API 1
├── Applications
└── Plugin Command Sets

API 2
└── Files File Actions

API 3
└── Read-only Files Preview Providers
```

API 3 插件仍然可以使用 API 1 / API 2 已经引入的能力。

插件可以提供：

- Applications / 应用
- Read-only Plugin Command Sets
- Files 右键 File Actions
- Files Preview Providers
- 插件自己的独立 runtime / 数据 /许可证文件

插件启停与管理：

- Refresh
- Properties
- Enable
- Disable
- Uninstall

禁用插件不会继续贡献 Application、Command Set、File Action 或 Preview Provider。

## Process Boundary

Workbench Core **不会 `dlopen()` 插件代码**。

Application 和交互式 File Action 使用独立进程运行。Workbench 会在需要交互时把控制终端的前台 process group 临时交给插件，并在插件退出后安全收回，因此插件可以正常使用 `termios` / raw / noncanonical 模式。

正常 `exit 0` 的 Application 会直接返回 Applications 页面；启动失败、异常信号或非零退出才显示结果提示。

Preview list / materialize 使用非交互式独立进程协议，不抢占前台 TTY。

> 进程隔离不是安全沙箱。安装不可信插件仍然等价于以当前用户权限运行不可信软件。

## Plugin Command Sets

插件可以提供自己的 `.wbc`，并作为：

```text
[Plugin] [Read-only]
```

进入 Command Sets。

插件命令模板还可以使用：

```text
{plugin_root}
```

Workbench 会将它展开为经过 shell quoting 的插件绝对根路径，使同一插件既能安装在用户目录，也能安装在系统目录。

详细格式：

- [`docs/PLUGIN_FORMAT.md`](docs/PLUGIN_FORMAT.md)
- [`docs/EXTENSION_API.md`](docs/EXTENSION_API.md)
- [`docs/CORE_EXTENSION_POLICY.md`](docs/CORE_EXTENSION_POLICY.md)

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

Workbench 可以读取 `/etc/os-release` 显示系统检测结果，但检测结果只作为参考，不会自动切换发行版模式。

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
- Applications
- Plugins
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

## Sticky Reserved Row

v0.16.1 新增可选的 **Sticky reserved row / 粘滞预留行**。

默认关闭。开启后：

```text
Terminal row 1    Reserved
Terminal row 2+   Workbench UI
```

Workbench 自有界面整体向下预留一行，鼠标命中坐标、Workbench Files 可用区域和 Terminal 可用高度会同步调整，而不是只在个别页面插入空行。

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

插件 File Action、Preview Provider 和插件启动路径会在实际执行前重新验证插件状态、descriptor 与 executable，降低 stale registry 或插件目录被替换后的风险。

---

# Lightweight by Design

Workbench 保持 C + libc / POSIX / Linux API 的轻量方向。

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

v0.16.1 当前 Linux x86_64 官方构建中的 `wb` 二进制约 **560 KB**；实际大小会随编译器、构建选项和版本变化。

设计目标是继续适合：

```text
512 MB RAM
低性能服务器 CPU
小型系统盘
SSH-only 环境
```

---

# External System Tools

Workbench 自己实现 UI、Files、Terminal、Command Sets、Plugin / Extension System、Profiles、参数系统和安全校验。

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

- 不属于 Workbench Core 源代码
- 默认不随 Workbench Core Release 分发
- 保持各自原有版权和许可证
- 是否存在取决于宿主 Linux

Workbench 仅仅可以调用某个独立程序，并不意味着该程序成为 Workbench 的一部分或被 Workbench 重新许可。

如果未来某个插件下载、携带或分发第三方 runtime，该插件必须单独履行对应第三方许可证和再分发义务。Workbench Core 的许可证不会自动覆盖第三方组件。

更多边界说明：

- [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md)
- [`DEPENDENCY_POLICY.md`](DEPENDENCY_POLICY.md)

---

# Install

## Prebuilt Linux x86_64 Binary

确认系统架构：

```bash
uname -m
```

如果输出：

```text
x86_64
```

解压 Release：

```bash
tar -xzf workbench-v0.16.1-linux-x86_64.tar.gz
cd workbench-v0.16.1-linux-x86_64
```

先检查版本：

```bash
./wb --version
```

安装：

```bash
sudo ./install.sh
```

如果当前已经是 root：

```bash
./install.sh
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
tar -xzf workbench-v0.16.1-source.tar.gz
cd workbench-v0.16.1
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

之后：

```bash
wb
```

---

# Configuration & User Data

主要用户数据位置：

```text
~/.config/workbench/config
~/.local/share/workbench/command-sets/
~/.local/share/workbench/plugins/
```

系统插件默认位置：

```text
/usr/local/share/workbench/plugins/
```

Workbench Terminal 的短期日志默认只存在内存中，不自动写入文件。

插件自己的配置、runtime 和数据位置由插件定义；插件不得假定当前工作目录就是插件目录。

---

# Project Structure

核心目录大致为：

```text
modules/
├── linux-core/
├── centos/
├── files-manager/
├── terminal/
├── command-sets/
├── plugins/
└── extensions/

include/
docs/
examples/
tests/
```

职责：

```text
Workbench Core
├── TUI / unified input
├── Settings
├── Search / parameters / risk
├── Module & profile integration
├── Plugin lifecycle
├── Extension registry
└── Execution

Workbench Files
├── POSIX/Linux filesystem operations
├── Plugin File Actions
└── Read-only Preview UI

Workbench Terminal
└── PTY + terminal state + short history

Command Sets
├── Linux system set [read-only]
├── User .wbc sets [editable]
└── Plugin .wbc sets [read-only]

Plugin System
├── Applications
├── Enable / Disable / Uninstall
├── API compatibility 1..3
└── Independent-process execution

Extension System
├── File Actions
└── Read-only Files Preview Providers
```

---

# Plugin Development

Workbench Core 不为单个产品增加专用兼容分支。Docker、7-Zip、Nginx、媒体工具、校验工具等应通过通用插件接口接入。

当前插件能力：

```text
Plugin API 1    Applications + Command Sets
Plugin API 2    + Files File Actions
Plugin API 3    + Read-only Files Preview Providers
```

示例和协议：

- [`docs/PLUGIN_FORMAT.md`](docs/PLUGIN_FORMAT.md)
- [`docs/EXTENSION_API.md`](docs/EXTENSION_API.md)
- [`examples/plugins/hello/`](examples/plugins/hello/)
- [`examples/plugins/file-actions-demo/`](examples/plugins/file-actions-demo/)

插件许可证可以与 Workbench Core 不同；插件作者必须自行确认其代码和任何随插件获取、携带或分发的第三方组件的许可证义务。

---

# Development Direction

v0.16.1 是正式版本。后续开发仍会保持“轻量 Core + 通用插件接口”的方向。

计划中的方向包括：

- 更多 Linux distribution profiles
- Docker / Podman 插件
- Git 插件
- Nginx / Web server tools
- Database tools
- System monitoring
- Terminal compatibility improvements
- 更完整的插件 SDK / 示例
- 事务式 Archive Edit / writable virtual-files API
- 更多通用 Preview Provider 类型

目标是逐步发展为适合低资源 Linux 的轻量、插件化文字操作环境，而不是堆成一个庞大的 Linux 发行版。

详细版本变化请查看 [`CHANGELOG.md`](CHANGELOG.md)。

---

# License & Usage

Workbench **不是 OSI 定义的开源项目**。

公开源代码、公开 GitHub 仓库、公开构建脚本、插件接口文档或可下载二进制，均不代表授予任意修改、派生开发、插件开发、重新分发或商业使用权。

除非 Workbench Project 另有明确书面授权，默认授权范围仅限于项目明确允许的官方 Workbench 下载与使用。

未经特别书面授权，不允许：

- 将 Workbench 用于公司、企业、学校、机构或其他组织用途
- 将 Workbench 用于商业用途、收费服务、托管服务或其他商业化活动
- 修改 Workbench 源代码后发布、分发或提供下载
- 提供 Workbench 的非官方修改版本、补丁版、重打包版本或衍生版本
- Fork 后以修改版本、兼容版本或衍生项目形式继续公开发布
- 未经授权重新分发 Workbench 源代码或二进制
- 将 Workbench 集成进其他可分发产品、系统镜像、商业产品或服务
- 删除、修改或规避 Workbench 的版权、许可、来源或官方身份声明
- 冒充 Workbench Project、官方版本、官方维护者或获得官方认可
- 基于 Workbench Plugin / Extension API 制作、发布、分发或提供第三方插件、二创插件、兼容插件或衍生扩展
- 修改官方插件后重新发布或提供修改版本
- 将官方插件、SDK、扩展格式或示例解释为对第三方插件开发、再分发或商业化的默认授权

未明确授予的权利全部保留。

## Source availability

Workbench 在 GitHub 等公开平台提供源码，主要用于：

- 查看和理解 Workbench 的实现
- 安全审计
- 学习项目架构
- 验证官方 Release 的来源和构建内容
- 按项目明确允许的方式自行构建和使用官方源码

源码公开本身不授予创建派生作品、发布修改版本、提供兼容实现或开发第三方插件的权利。

因此，Workbench 更准确的描述是：

```text
Source-available software
```

而不是：

```text
OSI-approved open source software
```

如果第三方网站、软件目录或镜像站收录 Workbench，不应将其许可证状态描述为 MIT、Apache、GPL、BSD 或其他 OSI 开源许可证。

## Plugins & Extensions

Workbench v0.16.1 提供 Plugin API / Extension API，并公开相关接口文档，用于官方开发、审核、兼容性验证以及经授权的扩展开发。

**公开插件接口不等于开放插件开发授权。**

除非 Workbench Project 另有明确书面授权：

- 不允许发布第三方 Workbench 插件
- 不允许发布二创、兼容或衍生插件
- 不允许修改官方插件后重新发布
- 不允许将 Workbench Plugin / Extension API 用于对外提供未经授权的兼容产品
- 不允许借助 Workbench 名称、插件格式或接口暗示获得官方认证或关联

未来某些插件、SDK、扩展项目可以获得独立特别授权；只有对应项目或授权文件明确写明时才适用，不自动扩展到 Workbench Core、其他插件或整个插件生态。

## Third-party code and tools

Workbench 的许可条款只适用于 Workbench Project 有权许可的代码和内容。

Workbench 调用的宿主系统程序、Linux / libc 组件，以及官方插件可能单独下载、携带或调用的第三方 runtime，继续受各自原始许可证约束。

Workbench 的许可证不会重新许可这些第三方组件，也不会改变第三方组件原有的版权与许可义务。

请同时查看：

- [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md)
- [`DEPENDENCY_POLICY.md`](DEPENDENCY_POLICY.md)

## Official and authorized plugins

官方或经特别授权发布的插件可以采用与 Workbench Core 不同的独立许可条款。

插件自己的 `LICENSE`、第三方通知、源码提供义务及 runtime 再分发条件，以该插件仓库或发行包中的明确文件为准。

官方插件具有独立许可证，不代表第三方自动获得制作、修改或发布 Workbench 插件的权利。

## Trademark & project identity

任何软件许可或源码公开均不授予使用 Workbench 名称、项目标识、官方插件名称或其他项目身份元素来暗示官方认证、赞助、授权或关联的权利。

第三方商标归各自权利人所有。

## Authorization

需要以下能力时，请先取得 Workbench Project 的明确书面授权：

- 企业、机构或商业使用
- 商业集成
- 重新分发
- 镜像或预装
- 修改版本发布
- 衍生项目
- 第三方插件或扩展开发与发布
- 官方插件修改或再发布
- 其他当前许可证没有明确授予的用途

正式使用、开发插件、修改或再分发前，如仓库根目录提供独立 `LICENSE`，以该文件及 Workbench Project 的明确书面授权为准。

---

# Contributions

欢迎提交：

- Issue
- Bug report
- 安全问题报告
- 文档错误报告
- 功能建议

代码、插件或其他实现性贡献是否接受，以及提交后适用的授权方式，以 Workbench Project 当时公布的贡献规则或单独书面约定为准。

公开仓库、公开 Issue、公开 Plugin API 或接受问题报告，不构成对修改版本、Fork、第三方插件或衍生项目发布权的授权。

---

# Copyright

Copyright © 2026 Workbench contributors.

Individual files and third-party components may carry their own copyright and license notices.
