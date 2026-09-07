# Changelog

## 0.15.0 - 2026-09-06

- 新增一级 **Command Sets / 指令集** 容器：首页不再直接进入 Linux 分类，而是 `指令集 → Linux 指令集 → 分类 → Action`。
- Linux 指令集正式建模为 `SYSTEM + Read-only`，模型层拒绝改名、删除、添加/编辑/删除 Action；CentOS Profile 仍作为 Linux 系统集的 overlay。
- 新增用户自定义指令集，可创建、重命名、编辑说明和删除；数据保存到 `~/.local/share/workbench/command-sets/`。
- 新增 Workbench 自有 `.wbc` 持久化格式；目录权限固定为 0700、文件为 0600，坏文件、重复 ID、系统 ID 覆盖会被安全跳过/拒绝。
- 单用户最多 64 个自定义指令集，每个最多 256 条自定义 Action。
- 自定义 Action 继续复用 Workbench `Task/Risk/ArgKind` 模型，支持标题、说明、关键词、风险等级、命令模板和 `{1}`～`{4}` 参数。
- 用户自定义指令支持添加、编辑、删除、搜索、属性、执行，键盘和鼠标两条路径均有真实 PTY 回归。
- 首页与 Terminal 复用同一个 Command Browser；首页使用 EXECUTE 模式，Terminal 使用 INSERT 模式，选择 Action 后只插入当前 Shell 输入行、不自动执行。
- 删除旧 Terminal 扁平“几百条 Action”选择器，系统 Linux Action 与用户自定义 Action 都从 Command Sets 一级页面进入。
- `--self-test` 新增 `command_sets_guard=PASS`；新增 Command Sets 核心、存储、Action CRUD 和键盘/鼠标/Terminal PTY 回归。
- source-only 发布继续保持不携带预编译 `wb`；`modules/command-sets/`、新测试和设计文档进入源码归档。

## 0.14.0 - 2026-09-06

- 新增第一方 **Workbench Terminal**，固定为首页第二项；Workbench UI 始终存在，Shell 运行在 Workbench 创建的 PTY 中。
- Terminal 优先启动宿主 `$SHELL`，不可用时回退 `/bin/sh`；Shell 作为独立子进程，不把 Bash/Zsh/Fish 等第三方 Shell 源码或二进制嵌入 `wb`。
- 新增 `modules/terminal/terminal.c` / `terminal.h`：PTY 生命周期、基础 ANSI/CSI/OSC 解析、屏幕可见边界、短期日志环形裁剪、resize、输入转发与子进程回收。
- 短期日志默认保留最近 1000 行，仅存在当前 Workbench 进程内、不写磁盘；每次历史上翻/下翻默认 50 行。
- 返回首页不会结束 Shell，后台输出仍持续泵入日志；再次打开 Terminal 默认保留上次屏幕。
- 设置新增 `terminal_log_lines`（100–10000，默认 1000）、`terminal_load_lines`（10–500，默认 50）和 `terminal_clear_on_open`（默认 0）。
- “重新打开时清屏”只改变可见边界，不删除短期日志；上翻历史仍可重新看到清屏前输出。
- Terminal 可打开 Workbench 指令集，将渲染后的 Action 命令插入 Shell 但不自动执行；另有输入、运行、清屏、历史、结束/重启 Shell 与返回控件。
- `Ctrl+]` 打开 Terminal 控制菜单；Terminal 页面将 `Ctrl+C` / `Ctrl+Z` 等控制字符转发给 PTY 内 Shell，离开后恢复 Workbench 自身信号行为。
- 修复 PTY 输出与键盘输入同一 `poll()` 周期到达时可能丢键的问题；修复 clear-on-reopen 与 PTY 尾部输出竞争导致旧画面重新出现的问题。
- README/THIRD_PARTY_NOTICES 明确 Workbench Terminal 自己实现，交互式 Shell 由宿主提供；没有新增 ncurses/libvterm/readline/tmux/xterm 等内嵌依赖。
- 源码发布继续使用 source-only `workbench-v0.14.0-source.tar.gz`，预编译 x86_64 二进制独立发布。

## 0.13.0 - 2026-09-06

- GitHub 发布边界正式收口：新增 `Workbench Source License 1.0` 自定义 source-available 授权文本。
- 新增 `THIRD_PARTY_NOTICES.md`，区分 Workbench 自有代码、Linux/POSIX/系统 C 运行时与宿主外部命令；外部工具不被描述为 Workbench 内嵌组件。
- 新增 `DEPENDENCY_POLICY.md`：默认不 vendor 第三方源码/二进制；GPL/AGPL 源码或库不得未经明确架构与许可证审计进入核心。
- README 新增“组件、外部工具与许可证边界”清单，公开 glibc/Linux、基础用户空间、iproute2、OpenSSH、BIND、压缩工具以及 CentOS Profile 的 DNF/RPM/systemd/firewalld/SELinux/NetworkManager 关系。
- README 与 notices 增加 CentOS 商标和非隶属声明；CentOS 仅作为兼容 Profile 描述。
- 源码归档更名为 `workbench-v0.13.0-source.tar.gz`，不再包含预编译 `wb`；架构二进制独立作为 Release asset。
- 新增发布策略与 source-only 打包回归测试；功能目录保持 Linux Generic 374 Actions、CentOS 483 Actions。

## 0.12.1 - 2026-09-06

- 设置新增 **虚拟键盘 / Virtual keyboard** 开关，默认关闭；实体键盘输入不受影响。
- 设置新增 **快捷键提示 / Shortcut hints** 开关，默认开启；关闭仅隐藏提示文本，不禁用快捷键或鼠标控件。
- 旧配置缺少新字段时自动采用 `virtual_keyboard=0`、`shortcut_hints=1`；两项均持久化到 Workbench 配置文件。
- Workbench Files 恢复完整快捷键图例，覆盖新建、查看、属性、复制、移动、重命名、删除、chmod、隐藏文件、Locations、Home 和返回桌面。
- 输入对话框在虚拟键盘关闭时不再绘制字符键盘，仅保留实体键盘输入和鼠标可点的 OK/Cancel；开启后恢复完整鼠标虚拟键盘。
- Mouse-only 回归测试改为显式开启虚拟键盘；新增 UI preference PTY 测试验证默认值、鼠标切换、持久化和 Files 提示开关。
- Linux Generic 仍为 374 个 Action，CentOS 工作集仍为 483 个 Action；没有新增运行时依赖。

## 0.12.0 - 2026-09-06

- 新增 Workbench 第一方统一双输入架构：`UiEvent` 归一化键盘、SGR 鼠标点击/释放与滚轮，`UiHitRegion` 统一管理可点击控件。
- 建立全局交互规则：当前交互功能必须同时存在 Keyboard-only 和 Mouse-only 完成路径；快捷键与鼠标控件调用同一功能逻辑。
- 设置、Linux Profile、确认策略、指令分类/列表/属性、搜索、执行确认和命令输出返回全部接入统一鼠标/键盘路径。
- 移除交互式 `fgets(stdin)` / `getchar()` 输入，新增统一 raw-mode 文本输入器。
- 新增 Workbench 内置鼠标虚拟键盘，支持 ASCII 字母、数字、Linux 常用路径/命令符号以及 Shift、Space、Backspace、Clear、OK、Cancel。
- Workbench Files 补齐 Locations/Home、新建文件/目录、隐藏文件、语言、桌面返回、查看器、属性、确认框等鼠标等价入口；既有右键菜单和 POSIX 文件操作安全边界保持不变。
- 兼容历史 `y` + Enter 确认习惯：raw 事件确认后只吞掉可选换行，不吞掉真正的后续输入事件。
- 新增真实 PTY Mouse-only 设置、Mouse-only Files、虚拟键盘、Mouse-only 指令集与 Keyboard-only 端到端测试；发布测试保证不依赖单一输入设备。
- 自检新增 `dual_input_guard=PASS`；Linux Generic 继续保持 374 个 Action，CentOS 工作集继续保持 483 个 Action。
- 双输入层仍保持 libc/POSIX-only，没有新增 ncurses、GUI toolkit 或其他运行时依赖。

## 0.11.0 - 2026-09-06

- Workbench Files 默认进入 **Locations / 位置**，以 Linux mount point 方式呈现 Home、系统根目录、本地存储和网络存储。
- 直接解析 `/proc/self/mountinfo`，使用 `statvfs()` 显示容量/使用率与只读状态；不调用 `lsblk`、`mount`、`df` 或 `findmnt`。
- 默认隐藏 proc/sysfs/devtmpfs/devpts/tmpfs/cgroup 等系统伪文件系统，按 `h` 可显式查看；根目录 `/` 始终保留。
- 支持 NFS、CIFS/SMB、SSHFS、9p、Ceph、GlusterFS 等网络挂载分类，并处理 mountinfo 八进制转义与重复挂载点。
- Workbench Files 接入已有 SGR mouse：Locations 点击进入、文件行单击选择/双击打开、滚轮移动、顶部 Home/Locations 按钮和右键文件操作菜单。
- 鼠标入口复用既有 POSIX 文件操作与删除确认，不新增 shell 文件操作路径。
- 新增 mountinfo/过滤/容量单测及真实 PTY SGR 鼠标回归；自检新增 `locations_guard=PASS`。
- Linux Generic 继续保持 374 个 Action，CentOS 工作集继续保持 483 个 Action。

## 0.10.0 - 2026-09-06

- 新增第一方 **Workbench Files** 本地资源管理器，并固定为首页第一项、Linux/CentOS 指令集之前。
- 文件管理器保持 libc/POSIX-only，不引入 ncurses、Go、Rust、Python 或第三方文件管理器源码。
- 支持目录浏览、父目录导航、隐藏文件切换、属性查看和内置只读分页文本查看器。
- 支持新建文件/目录、递归复制、移动、重命名、删除和 chmod；文件操作全部直接调用文件系统 API，不通过 shell。
- 递归复制保留普通文件/目录权限并保持符号链接为符号链接；拒绝把目录复制到自身子目录。
- 删除逻辑不跟随符号链接；递归删除拒绝 `/`、`.`、`..` 和解析后为根目录的真实目录目标。指向 `/` 的符号链接可以安全删除链接本身。
- 文件名和内置查看器输出会过滤终端控制字符，避免文件内容/文件名向 TUI 注入 ANSI 控制序列。
- Workbench Files 删除操作无条件要求显式确认；符号链接 chmod 被禁用，避免意外修改链接目标。
- 新增文件管理器 POSIX 操作单测、安全约束测试和真实 PTY 交互测试。
- Linux Generic 继续保持 374 个 Action，CentOS 工作集继续保持 483 个 Action。

## 0.9.1 - 2026-09-06

- 为降低误判风险，移除可选择的 `auto` Profile；默认始终为 Linux Generic。
- `/etc/os-release` 检测结果保留在设置页，仅作为参考信息，绝不会自动启用 CentOS。
- CentOS overlay 只有在用户手动选择 CentOS 后才会启用并持久化。
- 兼容 v0.9.0：旧配置中的 `linux_profile=auto` 会安全降级为 Generic。
- `wb --set-profile auto` 现在返回错误，避免脚本继续启用旧的自动模式。
- 更新 PTY smoke test、Profile 持久化测试和发布元数据。

## 0.9.0 - 2026-09-06

- 新增 `generic / auto / centos` Linux Profile，默认继续使用 Generic。
- 自动检测读取 `/etc/os-release`，当前仅精确 `ID=centos` 启用 CentOS。
- 新增 CentOS Stream 9/10 overlay：110 个 Action，最终 CentOS 工作集 483 个 Action。
- CentOS 覆盖层包括 DNF/RPM、systemd/journal、firewalld、SELinux、NetworkManager 和 CentOS 系统操作。
- CentOS 与 Linux Core 在 UI 中混合显示；CentOS 来源轻量标记，属性页显示来源。
- 支持同 Task ID overlay 覆盖；首个覆盖为 `system.hostname`。
- 新增 Packages、Services、Firewall、SELinux 分类；Generic 模式因计数为 0 自动隐藏。
- 设置页新增 Linux 版本选择、检测结果与实际生效 Profile。
- `--self-test` 同时验证 Generic 374 条与 CentOS 483 条最终目录。
- 新增 profile/overlay/category/persistence 测试，并扩展 PTY smoke test 验证运行时切换。

## 0.8.0 - 2026-09-06

- 将 102 个硬编码 Action 从 `main.c` 拆入独立 `linux-core` 模块。
- 按系统、文件、文本、进程、网络、存储、权限、归档、用户九类拆分 Action 数据文件。
- 将 Linux 通用指令集扩展到 374 个 task-oriented Action。
- 参数上限由 2 扩到 4，并统一 shell quoting / 类型校验。
- 增加破坏性路径类型，递归删除/权限修改拒绝根目录和过宽路径。
- 新增 `wb --self-test`：ID、元数据、占位符、模板语法、安全模式和路径保护检查。
- 增加 module registry，桌面不再硬编码 Linux Core 菜单下标。
- 增加自动测试、PTY TUI smoke test 和发布打包目标。
- 保持 `linux-core` 的发行版无关边界，不引入 apt/dnf/pacman/systemd 专属项。
