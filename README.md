# Discovery

一个轻量级、跨平台的 C++ 局域网设备发现库，用于在同一子网内自动广播自身并发现其他节点。

## 功能特性

- 跨平台：支持 Windows、Linux、macOS
- 广播与组播：支持 UDP broadcast 和 multicast 两种发现方式
- 轻量：仅依赖 C++ 标准库和系统 socket API
- 线程安全：发送与接收运行在后台线程中，公开 API 可安全并发调用
- 可配置：支持端口、广播间隔、设备 TTL、自发现和去重模式等参数
- 可携带用户数据：每个节点可附带最多 4KB 的自定义字符串

## 构建

### 使用 CMake

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

### CMake 选项

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `discovery_BUILD_SHARED` | `OFF` | 构建共享库 |
| `discovery_BUILD_EXAMPLES` | `ON` | 构建示例程序 |

## 集成

### 作为子目录

```cmake
add_subdirectory(discovery)
target_link_libraries(your_target PRIVATE discovery::discovery)
```

### 通过 `find_package`

```cmake
find_package(discovery REQUIRED)
target_link_libraries(your_target PRIVATE discovery::discovery)
```

## 快速开始

```cpp
#include <iostream>

#include "discovery/discovery_peer.h"

int main() {
  discovery::PeerParameters params;
  params.setPort(12345);
  params.setApplicationId(1001);
  params.setCanDiscover(true);
  params.setCanBeDiscovered(true);

  discovery::Peer peer;
  if (!peer.start(params, "my-device-name")) {
    return 1;
  }

  for (const auto& discoveredPeer : peer.listDiscovered()) {
    std::cout << discovery::ipToString(discoveredPeer.ipPort().ip())
              << " - " << discoveredPeer.userData() << '\n';
  }

  peer.stopAndWait();
  return 0;
}
```

### 仅发现模式

```cpp
discovery::PeerParameters params;
params.setPort(12345);
params.setApplicationId(1001);
params.setCanDiscover(true);
params.setCanBeDiscovered(false);

discovery::Peer peer;
peer.start(params, "");
```

### 仅广播模式

```cpp
discovery::PeerParameters params;
params.setPort(12345);
params.setApplicationId(1001);
params.setCanDiscover(false);
params.setCanBeDiscovered(true);

discovery::Peer peer;
peer.start(params, "server-node");
```

### 使用组播

```cpp
discovery::PeerParameters params;
params.setPort(12345);
params.setApplicationId(1001);
params.setCanUseBroadcast(false);
params.setCanUseMulticast(true);
params.setMulticastGroupAddress(0xE00000FB);  // 224.0.0.251

discovery::Peer peer;
peer.start(params, "multicast-peer");
```

## API 参考

### `PeerParameters`

`PeerParameters` 用于配置节点角色、网络传输模式和生命周期参数。

| 方法 | 说明 |
|------|------|
| `setPort(uint16_t)` | 设置发现服务使用的端口 |
| `setApplicationId(uint32_t)` | 设置应用 ID，用于隔离不同业务 |
| `setCanDiscover(bool)` | 是否接收并发现其他设备 |
| `setCanBeDiscovered(bool)` | 是否广播自身存在 |
| `setCanUseBroadcast(bool)` | 是否启用 UDP 广播，默认 `true` |
| `setCanUseMulticast(bool)` | 是否启用 UDP 组播，默认 `false` |
| `setMulticastGroupAddress(uint32_t)` | 设置组播地址，主机字节序 |
| `setSendTimeout(std::chrono::milliseconds)` | 设置广播间隔，默认 5000ms |
| `setDiscoveredPeerTtl(std::chrono::milliseconds)` | 设置已发现设备 TTL，默认 10000ms |
| `setDiscoverSelf(bool)` | 是否将自己计入发现结果，默认 `false` |
| `setSamePeerMode(SamePeerMode)` | 设置去重模式：`kIp` 或 `kIpAndPort` |
| `validate()` | 校验参数是否合法，成功时返回空字符串 |

### `Peer`

`Peer` 是发现服务的主入口，负责启动、停止、广播与接收。

| 方法 | 说明 |
|------|------|
| `start(params, userData)` | 启动发现服务；失败时返回 `false` |
| `stop()` | 请求停止并立即返回 |
| `stopAndWait()` | 请求停止并阻塞等待后台线程退出 |
| `setUserData(string)` | 动态更新广播给其他设备的用户数据 |
| `listDiscovered()` | 返回当前已发现设备的快照列表 |

### `DiscoveredPeer`

`DiscoveredPeer` 表示一个已发现的远端节点。

| 方法 | 说明 |
|------|------|
| `ipPort()` | 返回节点的 IP 地址与端口 |
| `userData()` | 返回节点附带的用户数据 |
| `lastReceivedPacket()` | 返回最后一次更新用户数据的快照序号 |
| `lastUpdated()` | 返回最后收到数据包的时间戳（ms） |

### 辅助函数

```cpp
/// 将主机字节序 IPv4 地址转换为 "A.B.C.D"
std::string ipToString(uint32_t ip);

/// 将端点转换为 "A.B.C.D:port"
std::string ipPortToString(const IpPort& ipPort);

/// 判断两个设备列表在指定去重模式下是否等价
bool isSame(SamePeerMode mode, const std::list<DiscoveredPeer>& lhs,
            const std::list<DiscoveredPeer>& rhs);
```

## 协议说明

### 数据包格式

协议头由固定魔数 `DSCV` 和版本号组成，后续跟随固定长度字段与可变长用户数据。

```text
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

所有多字节整数均采用大端字节序。

### 字段说明

| 字段 | 大小 | 说明 |
|------|------|------|
| Magic | 4 字节 | 固定为 `DSCV` |
| Version | 1 字节 | 当前版本号，固定为 `1` |
| Reserved | 3 字节 | 保留字段，当前固定为 `0` |
| Packet Type | 1 字节 | 数据包类型 |
| Application ID | 4 字节 | 应用标识，仅相同 ID 的节点彼此可见 |
| Peer ID | 4 字节 | 节点随机 ID，用于区分不同实例 |
| Snapshot Index | 8 字节 | 单调递增的包序号，用于丢弃旧包 |
| User Data Size | 2 字节 | 用户数据长度，最大 4096 |
| User Data | 可变 | 用户自定义内容 |

### 数据包类型

| 类型 | 值 | 说明 |
|------|----|------|
| `kIAmHere` | 0 | 周期性存在广播 |
| `kIAmOutOfHere` | 1 | 主动下线广播 |

## 项目结构

```text
discovery/
├── include/
│   └── discovery/
│       ├── discovery_discovered_peer.h
│       ├── discovery_ip_port.h
│       ├── discovery_peer.h
│       ├── discovery_peer_parameters.h
│       └── discovery_protocol.h
├── src/
│   ├── discovery_ip_port.cpp
│   ├── discovery_peer_env.cpp
│   └── discovery_protocol.cpp
├── examples/
│   ├── CMakeLists.txt
│   └── main.cpp
├── cmake/
│   └── discoveryConfig.cmake.in
├── Doxyfile
└── CMakeLists.txt
```

## 注意事项

1. 请确保防火墙允许对应 UDP 端口的广播或组播流量。
2. 广播通常只在同一子网内有效；组播依赖网络设备支持。
3. 只有 `applicationId` 相同的节点才会互相发现。
4. 如果启用组播，请确保 `setMulticastGroupAddress()` 配置的是合法组播地址。
