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



### 配置 VLAN

```bash
# 在接口上配置 VLAN
sudo vppctl create sub-interfaces host-veth1 100
sudo vppctl set interface l2 bridge host-veth1.100 1
```

### 配置 MAC 学习

```bash
# 禁用 MAC 学习
sudo vppctl set bridge-domain learn 1 disable

# 启用 MAC 学习
sudo vppctl set bridge-domain learn 1 enable
```

## 性能测试

### 使用 iperf3 测试吞吐量

```bash
# 在服务器端
sudo ip netns exec namespace-4 iperf3 -s

# 在客户端
sudo ip netns exec namespace-2 iperf3 -c 10.0.0.4 -t 30
```

### 使用 ping 测试延迟

```bash
# 测试延迟
sudo ip netns exec namespace-2 ping -c 100 -i 0.01 10.0.0.4 | tail -1
```

## VLAN 测试环境搭建

flow_table 插件支持 VLAN (802.1Q) 和 QinQ (802.1ad) 双层 VLAN。以下是 VLAN 测试环境的搭建方法。

### VLAN 网络拓扑

```
┌─────────────────────────────────────────────────────────────────┐
│  namespace-2 (客户端)                                            │
│                                                                  │
│  veth2 (物理接口)                                                │
│    ├── veth2.100 (VLAN 100)    - 10.0.100.2/24                 │
│    ├── veth2.200 (VLAN 200)    - 10.0.200.2/24                 │
│    └── veth2.100 (QinQ 100)                                     │
│         └── veth2.100.50 (QinQ 100.50) - 10.1.0.2/24           │
└─────────────────────────────────────────────────────────────────┘
                              │
                              │ VLAN tagged traffic
                              │
┌─────────────────────────────▼─────────────────────────────────┐
│                    Default Namespace                           │
│                                                                │
│         veth1 (接收 VLAN 标签)                                 │
│           │                                                    │
│           └──────────────┬─────────────────────────────────   │
│                          │                                    │
│                    ┌─────▼─────┐                              │
│                    │    VPP    │                              │
│                    │  Bridge   │  (VLAN-Aware)                │
│                    │  Domain 1 │                              │
│                    └─────┬─────┘                              │
│                          │                                    │
│         veth3 (发送 VLAN 标签)                                 │
└──────────────────────────┼────────────────────────────────────┘
                           │
                           │ VLAN tagged traffic
                           │
┌──────────────────────────▼──────────────────────────────────────┐
│  namespace-4 (服务器)                                            │
│                                                                  │
│  veth4 (物理接口)                                                │
│    ├── veth4.100 (VLAN 100)    - 10.0.100.4/24                 │
│    ├── veth4.200 (VLAN 200)    - 10.0.200.4/24                 │
│    └── veth4.100 (QinQ 100)                                     │
│         └── veth4.100.50 (QinQ 100.50) - 10.1.0.4/24           │
└─────────────────────────────────────────────────────────────────┘
```

### 环境搭建命令（单层 VLAN + QinQ）

创建一个包含所有类型的完整测试环境：

```bash
# 保持原有的无 VLAN 接口
# veth2: 10.0.0.2/24
# veth4: 10.0.0.4/24

# 添加单层 VLAN
sudo ip netns exec namespace-2 ip link add link veth2 name veth2.100 type vlan id 100
sudo ip netns exec namespace-2 ip addr add 10.0.100.2/24 dev veth2.100
sudo ip netns exec namespace-2 ip link set veth2.100 up

sudo ip netns exec namespace-4 ip link add link veth4 name veth4.100 type vlan id 100
sudo ip netns exec namespace-4 ip addr add 10.0.100.4/24 dev veth4.100
sudo ip netns exec namespace-4 ip link set veth4.100 up

# 添加 QinQ
sudo ip netns exec namespace-2 ip link add link veth2 name veth2.200 type vlan proto 802.1ad id 200
sudo ip netns exec namespace-2 ip link set veth2.200 up
sudo ip netns exec namespace-2 ip link add link veth2.200 name veth2.200.50 type vlan id 50
sudo ip netns exec namespace-2 ip addr add 10.2.0.2/24 dev veth2.200.50
sudo ip netns exec namespace-2 ip link set veth2.200.50 up

sudo ip netns exec namespace-4 ip link add link veth4 name veth4.200 type vlan proto 802.1ad id 200
sudo ip netns exec namespace-4 ip link set veth4.200 up
sudo ip netns exec namespace-4 ip link add link veth4.200 name veth4.200.50 type vlan id 50
sudo ip netns exec namespace-4 ip addr add 10.2.0.4/24 dev veth4.200.50
sudo ip netns exec namespace-4 ip link set veth4.200.50 up
```

### VLAN 配置总结

| 网络类型 | 客户端接口 | 客户端 IP | 服务器接口 | 服务器 IP | VLAN 类型 |
|---------|-----------|----------|-----------|----------|----------|
| 无 VLAN | veth2 | 10.0.0.2/24 | veth4 | 10.0.0.4/24 | - |
| VLAN 100 | veth2.100 | 10.0.100.2/24 | veth4.100 | 10.0.100.4/24 | 802.1Q |
| VLAN 200 | veth2.200 | 10.0.200.2/24 | veth4.200 | 10.0.200.4/24 | 802.1Q |
| QinQ 100.50 | veth2.100.50 | 10.1.0.2/24 | veth4.100.50 | 10.1.0.4/24 | 802.1ad |
