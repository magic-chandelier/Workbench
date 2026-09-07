# Workbench v0.15.0

极轻量、纯终端、简体中文 / English 双语 Linux 命令工作台。面向 SSH、服务器、救援环境、资源受限主机，以及希望通过“任务 → 参数 → 风险提示 → 执行”代替手搓长命令的用户。


## v0.15.0：Command Sets 指令集容器

v0.15.0 把原来直接挂在首页的 Linux 命令工作台提升为真正的一级 **Command Sets / 指令集** 容器。首页与 Workbench Terminal 使用同一个浏览器，系统预制与用户自定义指令从同一层级进入。

首页顺序：

```text
Workbench Files
Terminal
Command Sets / 指令集
Settings
Exit Workbench
```

Command Sets 第一层：

```text
Linux Commands                  [System] [Read-only]
My Server                       [Custom]
Nginx                           [Custom]
+ New Command Set
```

- **Linux Commands [System] [Read-only]** 包含现有 Linux Generic 374 Actions，并在用户手动启用 CentOS Profile 后形成 483 个有效 Actions；系统预制集合及其 Action 在模型层不可修改、删除或重命名。
- **用户自定义指令集** 可以新建、重命名、编辑说明和删除；其中的自定义 Action 可以添加、修改、删除、搜索、查看属性和运行。
- 用户数据保存在 `~/.local/share/workbench/command-sets/`，使用 Workbench 自有 `.wbc` 格式。目录使用 `0700`，文件使用 `0600`。损坏文件、重复 ID 和试图覆盖系统 `linux` ID 的文件不会进入可执行集合。
- 当前限制为最多 **64** 个用户指令集、每个最多 **256** 条用户 Action。
- 自定义 Action 继续复用 Workbench 原有参数/风险模型，可使用 `{1}`、`{2}`、`{3}`、`{4}` 参数模板。
- 首页进入 Command Sets 时使用 **EXECUTE** 模式；Terminal 打开同一个 Command Sets 页面时使用 **INSERT** 模式，渲染后的命令只插入当前 Shell，不会自动执行。
- 键盘与鼠标都可以管理自定义指令集；需要仅鼠标输入名称/命令时，可在 Settings 中开启虚拟键盘。

实现目录：

```text
modules/command-sets/
├── command_sets.c
├── command_sets.h
└── command_sets_ui.inc
```

## Command Sets

Workbench 的“指令集”是系统预制、未来官方插件和用户自定义命令的统一容器。当前来源规则：

| 来源 | 示例 | 修改 Action | 删除集合 |
|---|---|---:|---:|
| System | Linux Commands | 不允许 | 不允许 |
| User | 用户创建的 `.wbc` | 允许 | 允许 |
| Plugin | 未来官方/授权插件 | 由插件策略决定 | 通过插件生命周期管理 |

从首页选择 Action 会进入正常参数、风险确认和执行流程；从 Terminal 选择相同 Action 会进入参数渲染后返回 Shell 输入行，保持“先检查/修改，再按 Enter”的操作习惯。


## v0.14.0：GitHub 发布边界与组件透明度

v0.14.0 在 v0.13.0 的发布/许可证边界上新增 **Workbench 第一方内嵌 Terminal**，同时继续保持 source-only 源码归档与“第三方外部程序不随 Workbench 分发”的规则：

- 首页新增 **Terminal / 终端**，位于 Workbench Files 与 Linux/CentOS 指令集之间；Workbench UI 始终保持存在。
- Terminal 由 Workbench 自己通过 Linux/POSIX PTY 接口实现，不嵌入 xterm、tmux、screen、libvterm、ncurses、readline 等第三方终端模拟器源码/库。
- Terminal 内实际运行的交互式 Shell 来自宿主系统：优先使用可执行的 `$SHELL`，失败时回退 `/bin/sh`。Shell 作为独立子进程运行，不编译进 `wb`。
- 短期终端日志默认仅在当前 Workbench 进程内保留最近 **1000 行**，不写磁盘；每次“上翻/下翻”默认移动 **50 行**。
- 返回首页不会结束 Shell；再次打开 Terminal 默认保留上次屏幕。设置中的 `terminal_clear_on_open` 可改为重新打开时清屏，但清屏不会清除短期日志，仍可向上翻回旧输出。
- Terminal 可从 Workbench 指令集选择 Action，并把生成后的命令**插入 Shell 而不自动执行**，方便继续调整后手动运行。
- `Ctrl+]` 打开 Workbench Terminal 控制菜单；`Ctrl+C`/`Ctrl+Z` 等在 Terminal 页面按终端语义转发给内部 Shell，不用于终止 Workbench 主程序。
- 官方源码归档从 v0.15.0 起为 `workbench-v0.15.0-source.tar.gz`，仍然只包含源码、测试、文档与策略文件；预编译 `wb` 作为独立架构 Release asset 发布。

## v0.13.0：GitHub 发布边界与组件透明度

v0.13.0 不改变 Workbench 的功能行为，重点把 GitHub 发布、源码授权和上游边界收口清楚：

- Workbench 自己的代码采用 **Workbench Source License 1.0**（source-available，自定义授权），详见 [`LICENSE`](LICENSE)。
- 官方源码归档改为 `workbench-v0.13.0-source.tar.gz`，**只含源码、测试、文档和策略文件，不再夹带预编译 `wb`**；x86_64 二进制作为独立 Release asset 发布。
- [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md) 记录平台运行时、外部系统程序与 Workbench 的边界。
- [`DEPENDENCY_POLICY.md`](DEPENDENCY_POLICY.md) 固定依赖准入规则：默认不 vendor 第三方源码/二进制；GPL/AGPL 源码或库不得未经明确架构与许可证决策进入核心。
- README 公开列出 Workbench 可能调用的主要系统组件和外部工具，但这些程序**不随 Workbench 分发**，也不因被 Action 调用而成为 Workbench 源码的一部分。

## 组件、外部工具与许可证边界

Workbench 的发布边界分为三层。**下表中的“外部程序”均由宿主 Linux 提供，具体实现和许可证可能因发行版/软件包版本不同而变化；除 Workbench 自己的源码外，它们不随 Workbench 分发。**

| 层级 | 项目 / 组件 | Workbench 如何使用 | 是否打包进 Workbench |
|---|---|---|---|
| Workbench 自己实现 | TUI、双输入层、Workbench Files、Workbench Terminal/PTY/终端解析与短期日志、Linux/CentOS Action、Profile/overlay、安全校验 | 直接编译进 `wb` | **是，受 Workbench Source License 1.0 管理** |
| Linux 平台接口 | Linux kernel system calls、`/proc`、`/proc/self/mountinfo`、`/etc/os-release` | 正常 userspace API / 文件接口 | **否**；不包含 Linux kernel 源码 |
| C/POSIX 运行时 | libc / POSIX API；当前官方 x86_64 构建环境为 **glibc** | 动态运行时接口 | **否**；源码包不携带 glibc |
| POSIX shell | `/bin/sh` | 部分 Action 模板通过 shell 启动外部命令 | **否**；由宿主提供 |
| 交互式 Shell | `$SHELL`（如 `bash` / `zsh` / `fish`）或回退 `/bin/sh` | Workbench Terminal 通过 PTY 启动宿主 Shell 子进程 | **否**；不把 Shell 源码/二进制嵌入 `wb` |
| 基础用户空间工具 | GNU/core utilities、findutils、grep、sed、awk-family、util-linux、procps-ng 或发行版替代实现 | 文件、文本、进程、存储等 Action | **否** |
| 网络工具 | **iproute2** (`ip`, `ss`)、可选 legacy net-tools、ping/traceroute 实现 | 网络查看与配置 Action | **否** |
| HTTP/下载 | **curl / wget** | HTTP 请求、下载类 Action | **否** |
| SSH | **OpenSSH** (`ssh`, `scp`) | 远程连接/复制 Action | **否** |
| DNS | **BIND** utilities (`dig`, `host`, `nslookup`) 或发行版替代实现 | DNS 查询 Action | **否** |
| 归档/压缩 | **tar / gzip / bzip2 / xz / zip / unzip / cpio / ar** | 归档、压缩、解压 Action | **否** |
| CentOS 软件包 | **DNF / RPM** | 仅手动启用 CentOS profile 后调用 | **否** |
| CentOS 服务/日志 | **systemd** (`systemctl`, `journalctl`, `hostnamectl`) | CentOS overlay Action | **否** |
| CentOS 防火墙 | **firewalld** (`firewall-cmd`) | CentOS overlay Action | **否** |
| CentOS SELinux | SELinux userspace tools (`getenforce`, `setenforce`, `restorecon`, `semanage`) | CentOS overlay Action | **否** |
| CentOS 网络 | **NetworkManager** (`nmcli`) | CentOS overlay Action | **否** |

许可证关系的核心规则是：Workbench 可以执行宿主已安装的独立程序，但**默认不复制、vendor、静态嵌入或随 Release 打包这些第三方项目的源码/二进制**。如果未来需要把第三方库或源码真正加入编译/发布链，必须先按 `DEPENDENCY_POLICY.md` 做许可证与分发义务审计，并更新 `THIRD_PARTY_NOTICES.md`。

当前预编译 Linux x86_64 二进制使用系统 C 运行时；源码可在兼容的 Linux C/POSIX 环境中重新编译。`ldd`/具体 libc 结果以实际构建环境为准。

**CentOS trademark notice:** CentOS is a trademark of Red Hat, Inc. Workbench uses the name only to describe a compatibility profile. Workbench is not affiliated with or endorsed by Red Hat or the CentOS Project, and Workbench does not distribute CentOS logos.

## Workbench Terminal

Workbench Terminal 是 Workbench 自己实现的内嵌终端应用。Workbench UI 不退出，Shell 运行在 Workbench 创建的 PTY 中，Workbench 自己负责读取输出、基础终端解析、历史缓冲、鼠标/键盘控件和页面绘制。

首页顺序：

```text
Workbench Files
Terminal
Command Sets / 指令集
Settings
Exit Workbench
```

当前行为：

- 优先启动 `$SHELL` 指向的可执行 Shell；无效时安全回退 `/bin/sh`。
- Shell 是宿主系统的独立程序；Workbench 不内嵌 Bash、Zsh、Fish 或其他 Shell 源码。
- 返回首页只离开 Terminal 页面，**不会结束 Shell**；后台输出仍会被 Workbench 从 PTY 读取并进入短期日志。
- 短期日志默认最近 **1000 行**，只存在当前 Workbench 进程内；退出 Workbench 后释放，不自动写入磁盘。
- 每次“Older/Newer / 上翻/下翻”默认移动 **50 行**；鼠标滚轮也使用该设置。
- 默认重新打开 Terminal 时**不清屏**，方便继续查看上次调试位置。
- `terminal_clear_on_open=1` 时重新打开会从新的可见边界开始，但不会删除短期日志；继续上翻仍可查看清屏前输出。
- “Clear / 清屏”和“清日志”是不同概念；当前 UI 清屏只改变可见屏幕，不删除短期日志。
- Terminal 的“Commands / 指令集”打开与首页相同的 Command Sets 浏览器；可先选择 Linux 系统集或用户自定义集，再按原有层级选择 Action。渲染后的命令插入 Shell 输入行，**不会自动按 Enter 执行**。
- `Ctrl+]` 打开 Workbench 终端控制菜单；普通键盘输入直接发送给 Shell。
- Terminal 页面临时让 `Ctrl+C`、`Ctrl+Z` 等控制字符传入 PTY 内的 Shell；离开 Terminal 后恢复 Workbench 自己的终端信号行为。
- 鼠标可点击返回、指令集、输入、运行、上翻/下翻、清屏、结束/重启 Shell 等控件；虚拟键盘开启时可以通过 Workbench 输入器向 Shell 发送一整行文字。

设置中的 Terminal 参数：

```text
终端日志行数 / Terminal log lines        默认 1000，可设 100–10000
终端上滑加载 / Terminal history load      默认 50，可设 10–500
重新打开终端时清屏 / Clear on reopen      默认关闭
```

实现目录：

```text
modules/terminal/
├── terminal.c
└── terminal.h
```

Terminal 核心直接使用 Linux/POSIX 接口，例如 `posix_openpt()`、`grantpt()`、`unlockpt()`、`ptsname()`、`fork()`、`setsid()`、`ioctl()`、`termios`、`read()` / `write()` 和 `waitpid()`。当前没有嵌入第三方终端模拟器或 Shell 项目。

当前终端解析是 Workbench 自己的基础实现，覆盖普通文本、CR/LF/Tab/Backspace、部分 CSI/OSC、基础光标/清屏/清行与 PTY resize。它不是完整 xterm/vt100 仿真层；对 `vim`、`top`、`less`、`tmux` 等复杂全屏程序的兼容性会在后续版本逐步完善。

## v0.12.1：输入偏好与快捷键提示

v0.12.1 在统一双输入架构上增加两个可持久化 UI 偏好：

- **虚拟键盘 / Virtual keyboard**：默认 **关闭**。实体键盘输入始终可用；需要在键盘故障时只用鼠标输入文本的用户，可先在设置中用鼠标开启虚拟键盘。开启后，搜索、文件名、路径和 Action 参数继续使用 v0.12 的内置鼠标虚拟键盘。
- **快捷键提示 / Shortcut hints**：默认 **开启**。关闭只隐藏各页面的键盘/鼠标操作说明，不禁用任何快捷键或鼠标控件。
- **Workbench Files 快捷键提示恢复**：文件浏览器底部重新显示完整操作提示，包括 `n/N` 新建、`v` 查看、`p` 属性、`c` 复制、`m` 移动、`r` 重命名、`d` 删除、`x` 权限、`h` 隐藏文件、`g` Locations、`~` Home 和 `q` 桌面；该提示受全局“快捷键提示”开关控制。
- 两项配置写入 `~/.config/workbench/config`，旧配置缺少字段时自动采用安全默认值：虚拟键盘关闭、快捷键提示开启。

设置页现在包含：

```text
界面语言
Linux 版本
执行确认策略
虚拟键盘          已关闭（默认）
快捷键提示        已启用（默认）
终端登录自启动
返回主菜单
```

## v0.12.0：统一双输入架构

v0.12 把键盘与鼠标从各页面的零散判断收敛成 Workbench 自己的统一交互层。核心规则是：**任何 Workbench 自有交互功能都必须同时存在 Keyboard-only 与 Mouse-only 完成路径**。页面处理的是“打开、确认、返回、搜索、执行”等动作，而不是各自维护一套键值和鼠标坐标逻辑。

- **Keyboard-only**：不发送任何鼠标事件，仍可完成首页、设置、Linux/CentOS 指令集、搜索、属性、执行确认、Workbench Files 和全部文本输入。
- **Mouse-only**：在终端支持 SGR/xterm mouse reporting 时，不发送普通键盘字符也可完成同样的导航与操作。
- 统一 `UiEvent` 将键盘、鼠标点击、释放和滚轮归一化；统一 `UiHitRegion` 管理可点击目标。
- 设置页、Linux 版本选择、确认策略、指令分类/列表/属性、搜索、命令确认与命令结束返回均提供鼠标等价入口，同时保留原有快捷键。
- 所有交互式文字输入统一走 Workbench 内置输入器，不再用 `fgets(stdin)` / `getchar()` 临时切出 TUI。
- **鼠标虚拟键盘**覆盖 ASCII 字母、数字和常见 Linux 路径/命令符号，并提供 Shift、Space、Backspace、Clear、OK、Cancel；v0.12.1 起默认关闭，可在设置中开启；实体键盘始终可直接输入。
- Workbench Files 的 Locations、Home、新建、隐藏文件、语言、返回桌面、属性、查看器、右键操作和确认窗口都可单独用鼠标完成。
- 中文/Unicode 文件名仍可浏览和操作；Mouse-only 模式不内置中文输入法，因此“仅用鼠标新输入任意中文文本”不属于本版能力。
- 双输入保证覆盖 **Workbench 自己的 TUI、参数输入与文件管理交互**；执行后接管终端的外部程序（例如 `sudo` 密码提示、`ssh` 交互或全屏终端程序）仍遵循它们自己的输入能力，Workbench 不伪造或截获密码输入。
- 仍保持 libc/POSIX-only 单二进制方向，没有因为双输入层引入 ncurses、GUI toolkit、Python/Go/Rust 运行时。

真实 PTY 发布回归包含两条独立门槛：

```text
Keyboard-only: 不发送 SGR mouse 序列，完成设置、指令搜索/属性、Files 新建/chmod/重命名/删除
Mouse-only:    不发送普通键盘字符；先在设置中开启虚拟键盘，再完成 Profile、指令搜索/执行、Files 新建/权限/重命名/属性/删除
```

快捷键仍是高效率入口，但不会再存在“只能通过快捷键访问”的核心功能；鼠标按钮也不会成为键盘用户无法调用的唯一入口。

## v0.11.0：Workbench Files Locations + 鼠标交互

v0.11 在第一方 **Workbench Files** 上增加 Linux 原生 `Locations / 位置` 首页和完整鼠标交互。进入资源管理器时先看到 Home、系统根目录、本地挂载与网络挂载；多磁盘按 Linux mount point 显示，不伪造 Windows 盘符。文件浏览和文件变更继续直接使用 POSIX/Linux 文件系统 API。

首页顺序：

```text
Workbench Files
Linux Generic / CentOS Linux Command Set
Settings
Exit Workbench
```

## Workbench Files

Workbench Files 面向 SSH/服务器环境，保持纯终端、简体中文 / English 双语和单二进制目标。当前支持：

- **Locations / 位置**：进入 Files 默认先显示 Home、`/`、本地存储和网络存储
- 多磁盘按实际 Linux 挂载点显示，例如 `/data`、`/backup`、`/mnt/nas`，不使用 `C:` / `D:` 盘符
- 直接解析 `/proc/self/mountinfo` 发现挂载点，并用 `statvfs()` 显示总容量、使用比例与只读/读写状态
- 默认过滤 `proc`、`sysfs`、`devtmpfs`、`devpts`、`tmpfs`、`cgroup` 等系统/伪文件系统；Locations 中按 `h` 可显示
- NFS、CIFS/SMB、SSHFS、9p、Ceph、GlusterFS 等会作为网络存储识别
- 鼠标左键点击 Locations 可直接进入；文件列表单击选择、双击打开/查看、右键打开文件操作菜单；滚轮上下移动
- 浏览器顶部 `[g Locations]` 与 `[~ Home]` 可用键盘或鼠标跳转
- 浏览目录、进入目录、返回父目录
- 隐藏文件显示/隐藏
- 文件/目录/符号链接属性
- 内置只读分页文本查看器；检测到 NUL 的二进制内容不会直接输出到终端
- 新建普通文件和目录
- 复制普通文件、目录和符号链接；目录递归复制，符号链接保持为链接
- 移动和重命名
- 删除文件/符号链接，以及显式确认后的递归目录删除
- 修改普通文件/目录八进制权限
- 默认显示完整 Files 快捷键提示；可在设置中通过“快捷键提示”全局隐藏

常用按键：

```text
g               返回 Locations / 位置
~               打开 Home / 主目录
↑/k, ↓/j       移动选择
Enter           打开目录 / 查看普通文件
←/Backspace     返回父目录
v               查看文件
p/→             属性
n / N           新建文件 / 新建目录
c               复制
m               移动
r               重命名
d               删除（始终确认）
x               chmod
h               隐藏文件开关
l               切换语言
q/Esc           返回首页
```

鼠标：

- Locations：左键点击位置直接进入，滚轮移动选择
- 文件浏览：左键单击选择，500 ms 内再次点击同一行会打开目录/查看文件
- 文件浏览：右键打开 `File actions / 文件操作` 菜单，可进入属性、复制、移动、重命名、删除和 chmod
- 顶部 Locations/Home 按钮可直接点击

安全边界：

- 文件管理器的创建/复制/移动/重命名/删除/chmod **不调用 `/bin/sh`**。
- 递归删除不跟随符号链接，并拒绝 `/`、`.`、`..` 以及解析为根目录的真实目录目标。
- 指向 `/` 的符号链接只会删除链接本身，不会跟随到根目录。
- 目录不能复制到自身或自身子目录。
- 复制会恢复源普通文件/目录权限，不因 Workbench 进程的 `umask` 改变副本权限。
- 文件名和文本查看器会过滤 ASCII 控制字符，防止 ANSI 终端注入。
- 当前版本不对符号链接执行 chmod，避免改变链接目标。
- Locations 只做发现和导航，不执行 mount、umount、格式化、修复或任何磁盘状态变更。

实现目录：

```text
modules/files-manager/
├── files_manager.c    # POSIX 文件模型与变更操作
├── files_manager.h
└── files_ui.inc       # Workbench 原生 TUI 接线
```

没有嵌入 `nnn`、`lf`、Yazi、Midnight Commander 等第三方文件管理器源码，也没有新增对应运行时依赖。

## Linux Profile 与 CentOS Overlay

v0.9 在 v0.8 的独立 `linux-core` 数据层之上加入发行版 Profile。发行版指令直接编译进单二进制，但默认隐藏；用户在设置中选择 Linux 版本后，Workbench 构建一个统一的有效指令集。

```text
Workbench Core
  └─ Linux Profile
       ├─ Linux Generic
       │    └─ linux-core (374 Action)
       └─ CentOS
            ├─ linux-core (374 Action)
            └─ centos overlay (110 Action, 1 override)
                 ↓
            effective catalogue (483 Action)
```

CentOS **不是第二个桌面大类**。选择 CentOS 后，通用 Linux Action 与 CentOS Action 混合在同一个工作台中；CentOS 来源只做轻量标记，属性页会明确显示来源。同 ID 的 CentOS Action 会直接覆盖通用实现，因此不会出现两条重复功能。

## Linux 版本设置

设置 → `Linux 版本 / Linux version`：

- **Linux 通用 / Linux Generic**：默认值，只启用 `linux-core`。
- **CentOS**：必须由用户手动选择后才启用 CentOS overlay。

Workbench 仍会读取 `/etc/os-release` 并在设置页显示检测结果，但**检测结果仅供参考，不会自动改变当前模式**。即使检测到 CentOS，新安装和旧的 `auto` 配置也保持 Linux Generic，直到用户手动选择 CentOS。

配置持久化到：

```text
~/.config/workbench/config
```

默认：

```text
language=zh
confirm_normal=0
confirm_sensitive=0
confirm_privileged=1
linux_profile=generic
virtual_keyboard=0
shortcut_hints=1
```

可用于诊断：

```bash
wb --profile-info
wb --catalogue-info generic
wb --catalogue-info centos
wb --visible-categories generic
wb --visible-categories centos
```

## Linux 通用指令集

`linux-core` 保持 **374 个 task-oriented Action**：

| 种类 | Action 数量 |
|---|---:|
| 系统 | 47 |
| 文件 | 75 |
| 文本 | 56 |
| 进程 | 37 |
| 网络 | 49 |
| 存储 | 36 |
| 权限 | 21 |
| 归档/压缩 | 25 |
| 用户/会话 | 28 |
| **合计** | **374** |

项目边界仍是：**Linux Kernel + POSIX shell + 常见基础用户空间**。`linux-core` 本身不假设 `dnf/systemctl/firewall-cmd/nmcli` 等发行版工具存在。

Linux Core 数据目录：`modules/linux-core/actions/`

```text
modules/linux-core/
├── linux_core.c
├── linux_core.h
└── actions/
    ├── system.inc
    ├── files.inc
    ├── text.inc
    ├── process.inc
    ├── network.inc
    ├── storage.inc
    ├── permission.inc
    ├── archive.inc
    └── user.inc
```

## CentOS Overlay

CentOS 第一版面向现代 **CentOS Stream 9 / 10** 管理方式，使用 DNF/RPM、systemd、firewalld、SELinux 和 NetworkManager。CentOS 7/YUM-era 兼容不作为本 Profile 的主要目标，后续可以单独加 Legacy Profile。

CentOS overlay 共 **110 个 Action**，其中 `system.hostname` 覆盖 Linux Core 的同 ID 实现，因此 CentOS 最终有效指令集是 **483 个 Action**。

CentOS 专属主要分类：

| 分类 | Action 数量 |
|---|---:|
| 软件包 / DNF / RPM | 34 |
| 服务 / journal | 22 |
| 防火墙 / firewalld | 15 |
| SELinux | 14 |
| NetworkManager 与 CentOS 系统操作 | 25 |
| **CentOS overlay 合计** | **110** |

Generic 模式下“软件包 / 服务 / 防火墙 / SELinux”这些零 Action 分类完全隐藏。CentOS 模式下才自动出现。

CentOS 数据目录：`modules/centos/actions/`

```text
modules/centos/
├── centos.c
├── centos.h
└── actions/
    ├── packages.inc
    ├── services.inc
    ├── firewall.inc
    ├── selinux.inc
    ├── network.inc
    └── system.inc
```

## Action 与显示

Action 仍然按“用户要完成什么”拆分，而不是“一条可执行文件 = 一个菜单项”。例如软件包管理会拆为搜索、详情、安装、卸载、更新、事务历史、仓库和 RPM 查询等不同任务。

CentOS 模式的任务列表中只有 CentOS 来源项显示轻量 `CentOS` 标记；Linux Core 行保持干净。属性页会显示：

- 功能名称与说明
- 分类
- **来源（Linux Core / CentOS）**
- 风险级别
- 当前执行确认策略
- Task ID
- 命令模板
- 参数说明

## 参数与执行

每个 Action 最多支持 4 个结构化参数。参数按类型校验并做 shell quoting，再代入命令模板；当前类型包括普通文本、路径、破坏性路径、无符号整数、端口和 PID。

具体指令页支持：

- `↑` / `k`、`↓` / `j`：移动
- `Enter`：执行
- `p` / `→`：属性
- `/`：搜索
- `←` / `Esc` / `q`：返回
- `s`：设置
- `l`：切换语言
- 鼠标点击 `[执行]` / `[属性]`

## 三档风险策略

- **普通 / Normal**：只读或较安全操作，默认直接执行。
- **敏感 / Sensitive**：联网或修改较轻状态，默认直接执行。
- **特权 / Privileged**：可能需要高权限或会明显改变系统，默认执行前询问。

递归删除、递归权限修改和 CentOS SELinux 递归 relabel 等使用破坏性路径保护，不接受 `/`、`//`、`.`、`..` 或解析后为根目录的目标。

## 自检

```bash
./wb --self-test
```

自检会分别构建并验证 Generic 与 CentOS 两套最终工作集：

- Action ID 唯一及 overlay 覆盖后无重复
- 分类、风险和来源元数据有效
- 参数占位符一致
- Generic 374 条与 CentOS 483 条命令模板全部通过 `/bin/sh -n -c`
- Generic 模式不存在 CentOS 来源 Action 或 CentOS-only 分类
- CentOS 模式存在 overlay 和 override
- 搜索语义与破坏性路径保护正常
- Locations 默认挂载过滤保留 `/` 且不暴露系统伪文件系统

当前摘要：

```text
SELFTEST PASS profiles=2 generic_actions=374 centos_actions=483 centos_source=110 overrides=1 max_args=4 generic_system=47 generic_files=75 generic_text=56 generic_process=37 generic_network=49 generic_storage=36 generic_permission=21 generic_archive=25 generic_user=28 centos_packages=34 centos_services=22 centos_firewall=15 centos_selinux=14 generic_syntax=374 centos_syntax=483 destructive_path_guard=PASS files_guard=PASS locations_guard=PASS search_checked=5
```

完整测试：

```bash
make test
```

包含配置/检测 fixture、overlay、动态分类、持久化、文件管理器 POSIX 操作测试，以及真实 PTY 下的 Profile 切换、Workbench Files 键盘操作、Locations 多磁盘导航和 SGR 鼠标交互 smoke test。

## 构建

```bash
make clean
make
./wb
```

直接编译：

```bash
cc -O2 -pipe -Wall -Wextra -Wpedantic -I. \
  main.c modules/linux-core/linux_core.c modules/centos/centos.c modules/files-manager/files_manager.c -o wb
strip wb
./wb
```

安装：

```bash
sudo make install
wb
```

打包：

```bash
make package
```

输出：

```text
dist/workbench-v0.11.0.tar.gz
```

## 运行依赖目标

- 单二进制
- 无 Python / Node / JVM 运行依赖
- 无 ncurses 运行依赖
- 无后台 daemon
- Workbench TUI 自身仅使用 libc / POSIX 系统接口

某个 Action 调用的外部工具是否可用，仍取决于目标 Linux 实际安装的软件。CentOS Profile 只在用户手动显式选择 CentOS 后显示这些专属工具；系统检测不会自动启用。
