# VPP 网桥测试环境搭建文档

## 环境概述

本文档描述如何搭建一个使用 VPP 作为网桥连接两个网络命名空间的测试环境。

### 网络拓扑

```
┌─────────────────────┐                    ┌─────────────────────┐
│  namespace-2        │                    │  namespace-4        │
│  (Client)           │                    │  (Server)           │
│                     │                    │                     │
│  veth2              │                    │  veth4              │
│  10.0.0.2/24        │                    │  10.0.0.4/24        │
└──────────┬──────────┘                    └──────────┬──────────┘
           │                                          │
           │ veth pair                                │ veth pair
           │                                          │
┌──────────┴──────────────────────────────────────────┴──────────┐
│                    Default Namespace                           │
│                                                                │
│         veth1                              veth3               │
│           │                                  │                 │
│           └──────────────┬───────────────────┘                 │
│                          │                                     │
│                    ┌─────▼─────┐                               │
│                    │    VPP    │                               │
│                    │  Bridge   │                               │
│                    │  Domain 1 │                               │
│                    └───────────┘                               │
└────────────────────────────────────────────────────────────────┘
```

## 环境配置

### 系统信息

- 操作系统: Linux (ARM64)
- VPP 版本: 最新开发版本
- 网络命名空间: namespace-2 (客户端), namespace-4 (服务器)
- 虚拟接口: veth1, veth2, veth3, veth4

### IP 地址分配

| 命名空间     | 接口   | IP 地址  | 角色         |
|-------------|-------|---------|-------------|
| namespace-2 | veth2 | 10.0.0.2/24 | 客户端   |
| namespace-4 | veth4 | 10.0.0.4/24 | 服务器   |
| default     | veth1 | 无 IP       | VPP 接管 |
| default     | veth3 | 无 IP       | VPP 接管 |

## 搭建步骤

### 步骤 1: 创建网络命名空间

```bash
# 创建客户端命名空间
sudo ip netns add namespace-2

# 创建服务器命名空间
sudo ip netns add namespace-4
```

### 步骤 2: 创建 veth 接口对

```bash
# 创建客户端 veth 对: veth1 (默认命名空间) <-> veth2 (namespace-2)
sudo ip link add veth1 type veth peer name veth2
sudo ip link set veth2 netns namespace-2

# 创建服务器 veth 对: veth3 (默认命名空间) <-> veth4 (namespace-4)
sudo ip link add veth3 type veth peer name veth4
sudo ip link set veth4 netns namespace-4
```

### 步骤 3: 配置命名空间内的接口

```bash
# 配置客户端命名空间
sudo ip netns exec namespace-2 ip addr add 10.0.0.2/24 dev veth2
sudo ip netns exec namespace-2 ip link set veth2 up
sudo ip netns exec namespace-2 ip link set lo up

# 配置服务器命名空间
sudo ip netns exec namespace-4 ip addr add 10.0.0.4/24 dev veth4
sudo ip netns exec namespace-4 ip link set veth4 up
sudo ip netns exec namespace-4 ip link set lo up
```

### 步骤 4: 启动默认命名空间的 veth 接口

```bash
# 启动 veth1 和 veth3
sudo ip link set veth1 up
sudo ip link set veth3 up
```

### 步骤 5: 启动 VPP

```bash
# 进入 VPP 源码目录
cd /home/echo/vpp

# 启动 VPP (debug 模式)
make run

```

### 步骤 6: 配置 VPP 网桥

在 VPP 启动后，在 VPP CLI 中执行以下命令：

```bash
# 创建 host 接口（接管 veth1 和 veth3）
create host-interface name veth1
create host-interface name veth3

# 启动接口
set interface state host-veth1 up
set interface state host-veth3 up

# 创建网桥域（Bridge Domain）
create bridge-domain 1

# 将接口添加到网桥域
set interface l2 bridge host-veth1 1
set interface l2 bridge host-veth3 1
```

或者使用 vppctl 命令（VPP 在后台运行时）：

```bash
sudo vppctl create host-interface name veth1
sudo vppctl create host-interface name veth3
sudo vppctl set interface state host-veth1 up
sudo vppctl set interface state host-veth3 up
sudo vppctl create bridge-domain 1
sudo vppctl set interface l2 bridge host-veth1 1
sudo vppctl set interface l2 bridge host-veth3 1
```

## 验证和测试

### 验证 VPP 配置

```bash
# 查看接口状态
sudo vppctl show interface

# 查看网桥域详情
sudo vppctl show bridge-domain 1 detail

# 查看接口地址
sudo vppctl show interface address
```

预期输出示例：

```
              Name               Idx    State  MTU (L3/IP4/IP6/MPLS)     Counter          Count
host-veth1                        1      up          9000/0/0/0
host-veth3                        2      up          9000/0/0/0

  BD-ID   Index   BSN  Age(min)  Learning  U-Forwrd   UU-Flood   Flooding  ARP-Term  arp-ufwd   BVI-Intf
    1       1      0     off        on        on       flood       on       off       off        N/A

           Interface           If-idx ISN  SHG  BVI  TxFlood        VLAN-Tag-Rewrite
         host-veth1              1     1    0    -      *                 none
         host-veth3              2     1    0    -      *                 none
```

### 测试连通性

#### 1. 从客户端 ping 服务器

```bash
sudo ip netns exec namespace-2 ping -c 3 10.0.0.4
```

预期输出：

```
PING 10.0.0.4 (10.0.0.4) 56(84) bytes of data.
64 bytes from 10.0.0.4: icmp_seq=1 ttl=64 time=0.123 ms
64 bytes from 10.0.0.4: icmp_seq=2 ttl=64 time=0.089 ms
64 bytes from 10.0.0.4: icmp_seq=3 ttl=64 time=0.095 ms

--- 10.0.0.4 ping statistics ---
3 packets transmitted, 3 received, 0% packet loss, time 2048ms
rtt min/avg/max/mdev = 0.089/0.102/0.123/0.014 ms
```

#### 2. 从服务器 ping 客户端

```bash
sudo ip netns exec namespace-4 ping -c 3 10.0.0.2
```

#### 3. 测试 TCP 连接

在服务器端启动监听：

```bash
sudo ip netns exec namespace-4 nc -l 10.0.0.4 8080
```

在客户端连接：

```bash
sudo ip netns exec namespace-2 nc 10.0.0.4 8080
```

#### 4. 查看 VPP 统计信息

```bash
# 查看接口统计
sudo vppctl show interface host-veth1
sudo vppctl show interface host-veth3

# 查看错误统计
sudo vppctl show errors

# 查看 L2 FIB（转发信息库）
sudo vppctl show l2fib verbose
```

### 测试 flow_table 插件

如果启用了 flow_table 插件，可以进行以下测试：

```bash
# 在 VPP CLI 中启用 flow_table
vnetfilter enable-disable host-veth1
vnetfilter enable-disable host-veth3

# 生成一些流量
sudo ip netns exec namespace-2 ping -c 10 10.0.0.4

# 查看流表
show flow-table
```

## 自动化脚本

### 完整环境搭建脚本

已创建以下脚本文件：

1. **`/tmp/setup_vpp_bridge.sh`** - 网络命名空间和 veth 接口设置
2. **`/tmp/vpp_bridge_config.sh`** - VPP 网桥配置
3. **`/tmp/test_connectivity.sh`** - 连通性测试
4. **`/tmp/complete_setup.sh`** - 完整设置流程

### 使用自动化脚本

```bash
# 1. 设置网络环境
sudo /tmp/setup_vpp_bridge.sh

# 2. 启动 VPP
cd /home/echo/vpp
make run

# 3. 在另一个终端配置 VPP（VPP 运行后）
sudo /tmp/vpp_bridge_config.sh

# 4. 测试连通性
sudo /tmp/test_connectivity.sh
```

## 清理环境

```bash
# 停止 VPP
# 在 VPP CLI 中按 Ctrl+C 或执行 quit

# 删除网络命名空间
sudo ip netns delete namespace-2
sudo ip netns delete namespace-4

# 删除 veth 接口（会自动删除）
sudo ip link delete veth1 2>/dev/null || true
sudo ip link delete veth3 2>/dev/null || true
```

## 高级配置

### 启用 flow_table 插件

在 VPP 启动配置中添加：

```
plugins {
  plugin flow_table_plugin.so { enable }
}
```

或在运行时加载：

```bash
sudo vppctl load_plugin flow_table_plugin.so
```
