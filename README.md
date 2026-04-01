# Discovery

一个轻量级、跨平台的 C++ 库，用于在局域网中自动发现和识别网络设备。

## ✨ 功能特性

- **跨平台支持** - 支持 Windows 和 POSIX 系统 (Linux, macOS)
- **UDP 广播/组播** - 支持广播和组播两种发现模式
- **轻量级** - 无外部依赖，仅使用 C++ 标准库和系统 socket API
- **线程安全** - 内置线程安全的发送和接收机制
- **灵活配置** - 可配置发现超时、TTL、端口等参数
- **自定义用户数据** - 支持携带最多 4KB 的自定义用户数据进行设备识别

## 📦 安装

### 使用 CMake

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

### CMake 选项

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `discovery_BUILD_SHARED` | `OFF` | 构建动态库 |
| `discovery_BUILD_EXAMPLES` | `ON` | 构建示例程序 |

### 集成到项目

#### 方式一：作为子目录

```cmake
add_subdirectory(discovery)
target_link_libraries(your_target PRIVATE discovery::discovery)
```

#### 方式二：find_package

```cmake
find_package(discovery REQUIRED)
target_link_libraries(your_target PRIVATE discovery::discovery)
```

## 🚀 快速开始

### 基本使用

```cpp
#include "discovery/discovery_peer.h"

int main() {
    // 配置参数
    discovery::PeerParameters params;
    params.set_port(12345);
    params.set_application_id(1001);
    params.set_can_discover(true);       // 可以发现其他设备
    params.set_can_be_discovered(true);  // 可以被其他设备发现

    // 创建并启动 Peer
    discovery::Peer peer;
    if (!peer.Start(params, "my-device-name")) {
        return 1;
    }

    // 获取已发现的设备列表
    auto discovered = peer.ListDiscovered();
    for (const auto& p : discovered) {
        std::cout << "Found peer: "
                  << discovery::IpToString(p.ip_port().ip())
                  << " - " << p.user_data() << std::endl;
    }

    // 停止并等待线程结束
    peer.StopAndWaitForThreads();
    return 0;
}
```

### 仅发现模式

```cpp
discovery::PeerParameters params;
params.set_port(12345);
params.set_application_id(1001);
params.set_can_discover(true);
params.set_can_be_discovered(false);  // 不广播自己

discovery::Peer peer;
peer.Start(params, "");
```

### 仅广播模式

```cpp
discovery::PeerParameters params;
params.set_port(12345);
params.set_application_id(1001);
params.set_can_discover(false);       // 不接收
params.set_can_be_discovered(true);   // 只广播自己

discovery::Peer peer;
peer.Start(params, "server-node");
```

### 使用组播

```cpp
discovery::PeerParameters params;
params.set_port(12345);
params.set_application_id(1001);
params.set_can_use_broadcast(false);
params.set_can_use_multicast(true);
params.set_multicast_group_address(0xE00000FB);  // 224.0.0.251

discovery::Peer peer;
peer.Start(params, "multicast-peer");
```

## 📖 API 参考

### PeerParameters

配置发现服务的参数类。

| 方法 | 说明 |
|------|------|
| `set_port(uint16_t)` | 设置发现服务使用的端口 |
| `set_application_id(uint32_t)` | 设置应用 ID，用于区分不同应用 |
| `set_can_discover(bool)` | 是否可以发现其他设备 |
| `set_can_be_discovered(bool)` | 是否可以被其他设备发现 |
| `set_can_use_broadcast(bool)` | 是否使用广播（默认 `true`） |
| `set_can_use_multicast(bool)` | 是否使用组播（默认 `false`） |
| `set_multicast_group_address(uint32_t)` | 组播组地址（主机字节序） |
| `set_send_timeout(std::chrono::milliseconds)` | 广播间隔（默认 5000ms） |
| `set_discovered_peer_ttl(std::chrono::milliseconds)` | 设备 TTL（默认 10000ms） |
| `set_discover_self(bool)` | 是否发现自己（默认 `false`） |
| `set_same_peer_mode(SamePeerMode)` | 设备去重模式：`kIp` 或 `kIpAndPort` |

### Peer

核心发现服务类。

| 方法 | 说明 |
|------|------|
| `Start(params, user_data)` | 启动发现服务，返回 `false` 表示参数错误或 socket 初始化失败 |
| `Stop()` | 发送离线包后立即返回，后台线程自行结束 |
| `StopAndWaitForThreads()` | 发送离线包并阻塞至所有后台线程退出 |
| `SetUserData(string)` | 动态更新广播给其他设备的用户数据 |
| `ListDiscovered()` | 返回当前已发现设备的快照列表 |

### DiscoveredPeer

表示一个被发现的设备。

| 方法 | 说明 |
|------|------|
| `ip_port()` | 设备的 IP 地址和端口 |
| `user_data()` | 设备携带的用户数据 |
| `last_updated()` | 最后收到数据包的时间戳（ms） |

### 辅助函数

```cpp
// IP 地址（主机字节序）转 "A.B.C.D" 字符串
std::string IpToString(uint32_t ip);

// IP:Port 转 "A.B.C.D:port" 字符串
std::string IpPortToString(const IpPort& ip_port);

// 判断两个设备列表是否包含相同的设备集合
bool Same(SamePeerMode mode, const std::list<DiscoveredPeer>& lhv,
          const std::list<DiscoveredPeer>& rhv);
```

## 🔧 协议说明

### 数据包格式

Magic 为 4 字节 ASCII 字符串 `DSCV`，用于在 UDP 流量中快速识别协议包。

```
 0       1       2       3       4       5       6       7
+-------+-------+-------+-------+-------+-------+-------+-------+
|  'D'  |  'S'  |  'C'  |  'V'  |  Ver  |      Reserved        |
+-------+-------+-------+-------+-------+-------+-------+-------+
| PktType       |             Application ID                    |
+-------+-------+-------+-------+-------+-------+-------+-------+
|             Peer ID           |         Snapshot Index        |
+-------+-------+-------+-------+-------+-------+-------+-------+
|   Snapshot Index (cont.)      |       User Data Size          |
+-------+-------+-------+-------+-------+-------+-------+-------+
|                        User Data ...                          |
+-------+-------+-------+-------+-------+-------+-------+-------+
```

所有多字节整数字段均使用**大端序**。

| 字段 | 大小 | 说明 |
|------|------|------|
| Magic | 4 字节 | `DSCV`，协议标识 |
| Version | 1 字节 | 当前为 `1` |
| Reserved | 3 字节 | 保留，固定为 `0` |
| Packet Type | 1 字节 | 见下表 |
| Application ID | 4 字节 | 应用标识，仅同 ID 的设备互相可见 |
| Peer ID | 4 字节 | 随机生成，用于过滤自身发出的包 |
| Snapshot Index | 8 字节 | 单调递增，用于丢弃乱序到达的旧包 |
| User Data Size | 2 字节 | 用户数据长度，最大 4096 |
| User Data | 可变 | 应用自定义内容 |

### 包类型

| 类型 | 值 | 说明 |
|------|----|------|
| `IAmHere` | 0 | 周期性广播，宣告设备存在 |
| `IAmOutOfHere` | 1 | 设备主动下线时发送 |

## 📁 项目结构

```
discovery/
├── include/
│   └── discovery/
│       ├── discovery_peer.h            # 核心 Peer 类
│       ├── discovery_peer_parameters.h # 配置参数
│       ├── discovery_protocol.h        # 协议定义与序列化
│       ├── discovery_discovered_peer.h # 已发现设备
│       └── discovery_ip_port.h         # IP/端口工具
├── src/
│   ├── discovery_peer.cpp
│   ├── discovery_protocol.cpp
│   └── discovery_ip_port.cpp
├── examples/
│   └── main.cpp                        # 示例程序
├── tests/                              # 测试（待添加）
├── cmake/
│   └── discoveryConfig.cmake.in
├── CMakeLists.txt
└── README.md
```

## 🖥️ 平台支持

| 平台 | 状态 | 备注 |
|------|------|------|
| Windows 10+ | ✅ | VS 2017 15.7+ / VS 2019 / VS 2022，需要 ws2_32 库 |
| Linux | ✅ | GCC 7+ / Clang 5+ |
| macOS | ✅ | Xcode 10+ |

## ⚠️ 注意事项

1. **防火墙设置** - 确保防火墙允许 UDP 广播/组播流量
2. **端口占用** - 确保指定端口未被其他程序占用
3. **网络隔离** - 广播仅在同一子网内有效；组播需要网络设备支持 IGMP
4. **Application ID** - 使用相同 `application_id` 的设备才能相互发现

## 📝 示例程序

运行示例程序：

```bash
# 构建
cd build
cmake --build .

# 运行（参数：application_id port）
./examples/discovery_example 1001 12345
```

程序会持续监听并打印发现的设备及其用户数据变化。

## 🤝 贡献

欢迎提交 Issue 和 Pull Request！

## 📄 许可证

MIT License
