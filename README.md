# ChatSystem - 高并发即时通讯系统
基于 C++17 + Linux 网络编程 + Qt 构建的高性能即时通讯系统，摒弃传统多线程模型，采用 epoll 边缘触发的事件驱动架构，可稳定承载万级 TCP 长连接，支撑千万级消息可靠传输。

## 核心功能
1. **高性能网络架构**：基于 C++17 实现，采用 Linux 下 epoll 边缘触发的事件驱动架构，将 I/O 多路复用与非阻塞套接字深度结合，大幅降低上下文切换开销；
2. **可靠连接管理**：针对 TCP 长连接设计自适应心跳保活机制，动态调整心跳间隔、超时阈值与重连策略，弱网/高并发下有效识别死连接，避免资源泄漏；
3. **高效内存管理**：采用对象池、无锁队列与分片缓存策略，减少内存碎片与分配开销，提升内存使用效率；
4. **完整账号与消息体系**：搭建注册、登录及密码加密校验的账号体系，保障账号安全；基于 TCP 设计“消息头 + 消息体”封装格式，支持一对一双向实时消息交互，消息发送后实时回执，确保传输可靠；
5. **跨平台可视化客户端**：使用 Qt Widget 搭建客户端界面，包含登录窗口、聊天窗口、在线用户列表、消息记录展示等模块，支持 Linux/Windows 跨平台运行。

## 技术栈
| 模块         | 核心技术/框架                     |
|--------------|----------------------------------|
| 服务端开发   | C++17、Linux 网络编程             |
| 高并发网络   | epoll 边缘触发、非阻塞 IO、TCP 长连接 |
| 连接与内存管理 | 心跳保活机制、对象池、无锁队列、分片缓存 |
| 客户端界面   | Qt Widgets                       |
| 并发与性能   | 多线程、内存数据管理             |
| 编译构建     | qmake + Makefile                 |

## 项目业绩
- 稳定承载 **1 万 + TCP 长连接**，单机吞吐量达 18773 次/秒；
- 经 JMeter 压测验证，平均响应时间仅 2ms，错误率 0%，支撑千万级消息可靠传输；
- 客户端支持 Linux/Windows 跨平台运行，交互流程完整，账号与消息传输安全可靠。

## 环境要求
- 操作系统：Linux（Ubuntu 18.04+）/ Windows（客户端）
- 编译器：GCC 7.5+（支持 C++17）/ MSVC 2019+
- Qt 版本：Qt 5.12+（需安装 Qt Widgets 模块）
- 依赖：`libpthread`（Linux 多线程依赖）

### 环境快速配置（Ubuntu）
```bash
sudo apt update && sudo apt install gcc g++ make
sudo apt install qt5-default qttools5-dev-tools
sudo apt install libpthread-stubs0-dev
```
### 🚀 快速开始
### 1. 克隆代码仓库
```
git clone https://github.com/songdaoxiaoxiong/chatsystem.git
cd chatsystem
```
### 2. 编译运行服务端
```
cd server
make
./chat_server
```
### 3. 编译运行客户端
方式 1：Qt Creator 编译（推荐）
打开 Qt Creator，点击「文件」→「打开文件或项目」，选择 former_qt/chat.pro
点击「构建」按钮（锤子图标），等待编译完成
点击「运行」按钮（三角形图标）启动客户端
方式 2：命令行编译
```
cd ../former_qt
qmake chat.pro
make
./chat
```
### 📂 项目结构
```
chatsystem/
├── Makefile                # 全局编译脚本
├── former_qt/              # Qt 客户端核心目录
│   ├── chat.pro            # Qt 项目配置文件
│   ├── Global.h            # 客户端全局常量/数据结构
│   ├── main.cpp            # 客户端程序入口
│   ├── mainwindow/         # 主窗口模块
│   ├── login/              # 登录界面模块
│   ├── register/           # 注册界面模块
│   ├── chatclientui/       # 聊天核心界面
│   ├── client/             # 客户端网络模块
│   ├── qss/                # 样式表目录
│   ├── res/                # 静态资源目录
│   └── res.qrc             # Qt 资源配置文件
└── server/                 # 服务端核心目录
    ├── main.cpp            # 服务端程序入口
    ├── client/             # 客户端连接管理
    ├── heartbeat/          # 心跳保活模块
    ├── message/            # 消息处理模块
    ├── reactor/            # Reactor 模型实现
    ├── thread/             # 多线程模块
    └── utils/              # 工具类模块
```
## ✨扩展方向
### 客户端
- 新增文件 / 图片 / 表情发送功能
- 基于 SQLite 实现聊天记录存储
- 开发好友列表、群聊功能
- 支持暗黑模式、自定义主题
### 服务端
- 集成 MySQL 存储用户 / 聊天数据
- 新增 AES/RSA 消息加密
- 实现分布式部署支撑高并发
- 新增日志持久化、监控告警
