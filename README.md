# 🖥️ DesktopWord — 桌面单词

一个常驻 Windows 桌面的英语单词记忆工具，基于艾宾浩斯遗忘曲线科学复习。

<p align="center">
  <i>单词浮在桌面上，每次看桌面都能顺便背一个。</i>
</p>

## ✨ 功能

- **桌面常驻** — 单词显示在桌面右上方，位于所有窗口下方，不影响工作
- **多词同时显示** — 1~20 个单词可自由配置
- **定时刷新** — 可配置刷新间隔（默认 30 秒）
- **艾宾浩斯记忆法** — 1/2/4/7/15/30 天间隔科学复习
- **双击标记熟悉** — 双击已掌握的单词，永不再出现
- **悬停放大** — 鼠标悬停单词自动放大；可配置悬停时暂停计时
- **右键菜单** — 右键单词快速标记熟悉
- **词库管理** — 查看/筛选/标记词库中所有单词
- **自定义背景** — 支持透明背景或自定义图片 (PNG/JPG/BMP/GIF)
- **系统托盘** — 最小化到托盘，不占任务栏空间
- **导入词库** — 支持导入自定义 TXT 词库
- **学习进度持久化** — 退出后重启，熟悉的单词不再出现，学习进度继续
- **暗黑模式** — 跟随 Windows 系统主题自动切换

## 📸 截图

（请自行截图添加 `screenshots/` 目录）

## 🔧 构建

### 环境要求

- [w64devkit](https://github.com/skeeto/w64devkit)（推荐）或 MinGW-w64 (GCC ≥ 14)
- GNU Make
- Windows 10/11

### 编译

```bash
git clone https://github.com/你的用户名/DesktopWord.git
cd DesktopWord
make
```

编译产物：`DesktopWord.exe`（单文件，无需额外 DLL）

### 运行

```bash
./DesktopWord.exe
```

程序启动后会自动在右下角系统托盘显示图标。右击托盘图标打开菜单。

## 📖 使用说明

| 操作 | 方式 |
|------|------|
| 标记熟悉 | 双击桌面上的单词，或右键 → 标记为熟悉 |
| 打开设置 | 右击托盘图标 → 设置 |
| 导入词库 | 右击托盘图标 → 导入词库 |
| 词库管理 | 右击托盘图标 → 词库管理 |
| 查看统计 | 右击托盘图标 → 学习统计 |
| 显示/隐藏 | 左击托盘图标切换 |
| 退出程序 | 右击托盘图标 → 退出 |

## 📂 文件结构

```
DesktopWord/
├── main.c              # WinMain, 窗口过程, 事件处理
├── main.h              # 全局类型、常量、声明
├── word_store.c        # 词库加载、进度持久化
├── scheduler.c         # 艾宾浩斯调度算法
├── render.c            # GDI 文字渲染（ClearType + 阴影）
├── desktop_embed.c     # 桌面嵌入、窗口定位
├── tray.c              # 系统托盘
├── settings.c          # 设置对话框
├── word_bank.c         # 词库管理对话框
├── darkmode.c/h        # 暗黑模式支持
├── helpers.c           # 日期计算、路径工具
├── bg_image.cpp        # GDI+ 图片加载（C++，extern "C"）
├── resources.rc        # 嵌入 manifest（视觉样式）
├── DesktopWord.exe.manifest
├── words.txt           # 词库文件（单词|音标|释义）
├── Makefile
└── README.md
```

### 运行时生成的文件

| 文件 | 说明 |
|------|------|
| `progress.dat` | 学习进度（艾宾浩斯复习数据） |
| `familiar.dat` | 熟悉单词列表（UTF-8，可用记事本查看） |

## 📝 词库格式

`words.txt` 每行一个单词，格式：

```
单词|音标|释义
abandon|əˈbændən|v.放弃;抛弃
```

- 以 `#` 开头的行为注释
- 导入功能支持相同格式的外部词库
- 重复单词不会重复导入

## 🛠️ 技术栈

- **语言**：C（Win32 API）+ 少量 C++（GDI+ 桥接）
- **图形**：GDI+ + UpdateLayeredWindow（每像素 alpha 透明）
- **编码**：全部 W 后缀 API，UTF-8/UTF-16 手动转换
- **构建**：GCC (MinGW-w64) + GNU Make
- **依赖**：仅系统 DLL（gdi32, gdiplus, comctl32, uxtheme, dwmapi）

## 📄 许可

MIT License
