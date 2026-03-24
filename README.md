# Discovery

一个轻量级、跨平台的 C++ 库，用于在局域网中自动发现和识别网络设备。

## ✨ 功能特性

- **跨平台支持** - 支持 Windows 和 POSIX 系统 (Linux, macOS)
- **UDP 广播/组播** - 支持广播和组播两种发现模式
- **协议版本管理** - 内置协议版本协商，支持向后兼容
- **轻量级** - 无外部依赖，仅使用 C++ 标准库和系统 socket API
- **线程安全** - 内置线程安全的发送和接收机制
- **灵活配置** - 可配置发现超时、TTL、端口等参数
- **自定义用户数据** - 支持携带自定义用户数据进行设备识别

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
params.set_multicast_group_address(0xE0000001);  // 224.0.0.1

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
| `set_multicast_group_address(uint32_t)` | 组播地址 |
| `set_send_timeout(std::chrono::milliseconds)` | 广播间隔（默认 5000ms） |
| `set_discovered_peer_ttl(std::chrono::milliseconds)` | 设备 TTL（默认 10000ms） |
| `set_discover_self(bool)` | 是否发现自己（默认 `false`） |
| `set_same_peer_mode(SamePeerMode)` | 设备比较模式：`kIp` 或 `kIpAndPort` |
| `set_supported_protocol_versions(min, max)` | 设置支持的协议版本范围 |

### Peer

核心发现服务类。

| 方法 | 说明 |
|------|------|
| `Start(params, user_data)` | 启动发现服务 |
| `Stop()` | 停止服务（不等待线程） |
| `StopAndWaitForThreads()` | 停止服务并等待线程结束 |
| `SetUserData(string)` | 动态更新用户数据 |
| `ListDiscovered()` | 获取已发现的设备列表 |

### DiscoveredPeer

表示一个被发现的设备。

| 方法 | 说明 |
|------|------|
| `ip_port()` | 获取设备的 IP 和端口 |
| `user_data()` | 获取设备携带的用户数据 |
| `last_updated()` | 最后更新时间 |

### 辅助函数

```cpp
// IP 地址转字符串
std::string IpToString(uint32_t ip);

// IP:Port 转字符串  
std::string IpPortToString(const IpPort& ip_port);

// 比较两个设备列表是否相同
bool Same(SamePeerMode mode, const std::list<DiscoveredPeer>& lhv, 
          const std::list<DiscoveredPeer>& rhv);
```

## 🔧 协议说明

### 协议格式

支持两个协议版本：

#### Version 0 (Magic: `RN6U`)

```
+--------+--------+--------+--------+
|   R    |   N    |   6    |   U    |  Magic (4 bytes)
+--------+--------+--------+--------+
| Version|      Reserved (3 bytes)  |  Header
+--------+--------+--------+--------+
|    Packet Type  |  Application ID |
+--------+--------+--------+--------+
|     Peer ID     | Snapshot Index  |
+--------+--------+--------+--------+
| User Data Size  |  Padding Size   |
+--------+--------+--------+--------+
|           User Data ...           |  Payload
+--------+--------+--------+--------+
```

#### Version 1 (Magic: `SO7V`)

与 V0 类似，但移除了 Padding 字段，用户数据上限为 4KB。

### 包类型

| 类型 | 值 | 说明 |
|------|-----|------|
| `IAmHere` | 0 | 广播设备存在 |
| `IAmOutOfHere` | 1 | 通知设备离线 |

## 📁 项目结构

```
discovery/
├── include/
│   └── discovery/
│       ├── discovery_peer.h            # 核心 Peer 类
│       ├── discovery_peer_parameters.h # 配置参数
│       ├── discovery_protocol.h        # 协议定义
│       ├── discovery_protocol_version.h
│       ├── discovery_discovered_peer.h # 发现的设备
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
| Windows 10+ | ✅ | VS 2017 15.7+ / VS 2019 / VS 2022 / VS 2026，需要 ws2_32 库 |
| Linux | ✅ | GCC 7+ / Clang 5+ |
| macOS | ✅ | Xcode 10+ |

## ⚠️ 注意事项

1. **防火墙设置** - 确保防火墙允许 UDP 广播/组播流量
2. **端口占用** - 确保指定端口未被其他程序占用
3. **网络隔离** - 广播仅在同一子网内有效
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

程序会持续监听并打印发现的设备。

## 🤝 贡献

欢迎提交 Issue 和 Pull Request！

## 📄 许可证

MIT License
