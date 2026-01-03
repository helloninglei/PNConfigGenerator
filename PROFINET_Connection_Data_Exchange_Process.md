# PROFINET 主站与从站连接及数据交换详细说明

## 概述

PROFINET通信基于**以太网**，采用**实时通信（RT）**机制进行周期性数据交换，以及**RPC (Remote Procedure Call)** 机制进行非周期性控制和配置。整个过程可分为**配置阶段**、**连接建立阶段**、**参数化阶段**、**应用就绪阶段**和**周期性数据交换阶段**。

---

## 一、核心概念和术语

### 1.1 关键概念

| 概念 | 说明 |
|------|------|
| **AR (Application Relation)** | 应用关系，主站和从站之间的逻辑连接 |
| **IOCR (IO Connection Relation)** | IO连接关系，定义周期性数据交换的参数 |
| **AlarmCR (Alarm Connection Relation)** | 告警连接关系，用于非周期性事件通知 |
| **DCE RPC** | 分布式计算环境远程过程调用协议 |
| **RT (Real-Time)** | 实时以太网通信，EtherType 0x8892 |
| **DCP (Discovery and Configuration Protocol)** | 发现和配置协议 |
| **APDU Status** | 应用协议数据单元状态，包含周期计数器和数据状态 |
| **IOPS (IO Provider Status)** | IO提供者状态（数据发送方的状态） |
| **IOCS (IO Consumer Status)** | IO消费者状态（数据接收方的状态） |

### 1.2 状态机

```
ArState 枚举：
- Offline        : 未连接
- Connecting     : 正在建立AR连接
- Parameterizing : 正在参数化
- AppReady       : 等待应用就绪确认
- Running        : 周期性数据交换运行中
- Error          : 错误状态
```

---

## 二、连接建立和数据交换的完整流程

### 阶段 0：设备发现和配置 (DCP)

**目的**：在建立AR之前，通过DCP协议配置从站的站名和IP地址。

#### 0.1 设置站名

**主站操作**：
```cpp
scanner.setDeviceName(targetMac, stationName, true);
```

**DCP包结构**：
- **目的MAC**：从站MAC地址
- **源MAC**：主站MAC地址
- **EtherType**：0x8892 (PROFINET)
- **FrameID**：0xFEFC (DCP Set Request)
- **Service**：Set Request
- **Block**：NameOfStation
  - **BlockQualifier**：0x0001 (Permanent - 永久保存)
  - **StationName**：字符串形式的站名

**从站响应**：
- **FrameID**：0xFEFD (DCP Response)
- **Status**：OK/Error

**等待时间**：2000ms，确保从站完全提交并更新身份。

#### 0.2 设置IP地址

**主站操作**：
```cpp
scanner.setDeviceIp(targetMac, targetIp, mask, gateway, true);
```

**DCP包结构**：
- **Block**：IP Parameter
  - **BlockQualifier**：0x0001 (Permanent)
  - **IP Address**：4字节
  - **Subnet Mask**：4字节
  - **Gateway**：4字节

**等待时间**：1000ms

**关键参数**：
```cpp
QString mask = "255.255.255.0";          // 默认子网掩码
QString gateway = "192.168.0.1";          // 默认网关
```

---

### 阶段 1：建立AR连接 (Connect)

**状态**：`ArState::Connecting`

**目的**：主站向从站发送Connect请求，建立AR（应用关系）。

#### 1.1 发送Connect请求 (RPC Connect)

**传输协议**：UDP over IPv4（源端口和目标端口都是 0x8894）

**数据包结构**：

```
Ethernet Header (14 bytes):
  - Destination MAC: 从站MAC
  - Source MAC: 主站MAC
  - EtherType: 0x0800 (IPv4)

IP Header (20 bytes):
  - Version: 4
  - Protocol: 0x11 (UDP)
  - Source IP: 192.168.0.254（主站IP）
  - Destination IP: 从站IP

UDP Header (8 bytes):
  - Source Port: 0x8894 (34964)
  - Destination Port: 0x8894

DCE RPC Header (80 bytes):
  - RPC Version: 4
  - PDU Type: 0 (Request)
  - Flags1: 0x23 (Idempotent | Last Frag | First Frag)
  - Data Representation: 0x00 (Big Endian)
  - Interface UUID: DEA00001-6C97-11D1-8271-00A02442DF7D (PROFINET CM)
  - Activity UUID: 自定义
  - Interface Version: 1.1
  - Sequence Number: 递增
  - Opnum: 0x0000 (Connect)

RPC Body (NDR + Blocks):
  NDR Header (20 bytes):
    - Maximum: 512
    - Args Length: 计算后填充
    - Maximum Count: 同 Args Length
    - Offset: 0
    - Actual Count: 同 Args Length
```

#### 1.2 Blocks 详细结构

**Block 1: ARBlockRequest (Type 0x0101)**

```cpp
结构：
- BlockType: 0x0101
- BlockLength: 根据内容计算
- BlockVersionHigh: 0x01
- BlockVersionLow: 0x00
- ARType: 0x0001 (IO Controller AR)
- ARUUID: 16字节唯一标识符（如：01,02,03,...,16）
- SessionKey: 0x0001
- CMInitiatorMacAdd: 主站MAC地址（6字节）
- CMInitiatorObjectUUID: DEA00000-6C97-11D1-8271-00A02442DF7D
- ARProperties: 0x00000211
  - Bit 30: Supervisor Takeover Allowed
  - Bit 9: Device Access
  - Bit 4-0: Pull Module Alarm Allowed
- CMInitiatorActivityTimeoutFactor: 0x0064 (100，表示10秒超时)
- CMInitiatorUDPRTPort: 0x8894
- StationNameLength: 站名长度（2字节）
- CMInitiatorStationName: 站名字符串
```

**Block 2: IOCRBlockReq - Output (Type 0x0102)**

```cpp
结构：
- BlockType: 0x0102
- BlockLength: 根据内容计算
- BlockVersionHigh: 0x01
- IOCRType: 0x0002 (Output CR - 主站输出到从站)
- IOCRReference: 0x0002
- LT (FrameID): 0x8892
- IOCRProperties: 0x00000002 (RT_CLASS_2)
- DataLength: 0x0003 (1 byte 数据 + 1 byte IOPS + 1 byte IOCS)
- FrameID: 0x8002 (主站发送的周期帧ID)
- SendClockFactor: 0x0010 (16，表示周期 = 16 × 31.25μs = 500μs ≈ 0.5ms)
- ReductionRatio: 0x0010 (16)
- Phase: 0x0001
- Sequence: 0x0000
- FrameOffset: 0x00000000
- WatchdogFactor: 0x0003
- DataHoldFactor: 0x0003
- IOCRTagHeader: 0xC000 (VLAN Priority 6, VLAN ID 0)
- IOCRMulticastMACAdd: 00:00:00:00:00:00
- NumberOfAPIs: 0x0001
  - API[0]: 0x00000000
- NumberOfIODataObjects: 0x0001
  - SlotNumber: 0x0002
  - SubslotNumber: 0x0001
  - FrameOffset: 0x0000 (数据从帧偏移0开始)
- NumberOfIOCS: 0x0001
  - SlotNumber: 0x0002
  - SubslotNumber: 0x0001
  - FrameOffset: 0x0002 (IOCS在数据+IOPS之后，偏移2字节)
```

**Block 3: IOCRBlockReq - Input (Type 0x0102)**

```cpp
结构：（与Block 2类似，但方向相反）
- IOCRType: 0x0001 (Input CR - 从站输出到主站)
- IOCRReference: 0x0001
- FrameID: 0x8001 (从站发送的周期帧ID)
- DataLength: 0x0003
- 其他参数同Block 2
```

**Block 4: AlarmCRBlockReq (Type 0x0103)**

```cpp
结构：
- BlockType: 0x0103
- BlockLength: 0x0016 (22 bytes)
- AlarmCRType: 0x0001 (Alarm CR)
- LT: 0x0800
- AlarmCRProperties: 0x00000002 (Transport UDP)
- RTATimeoutFactor: 0x0064 (100)
- RTARetries: 0x0003
- LocalAlarmReference: 0x0001
- MaxAlarmDataLength: 0x00C8 (200 bytes)
- AlarmCRTagHeaderHigh: 0xC000
- AlarmCRTagHeaderLow: 0xA000
```

**Block 5: ExpectedSubmoduleBlockReq (Type 0x0104)**

```cpp
结构：
- BlockType: 0x0104
- NumberOfAPIs: 0x0001
  - API: 0x00000000
  - SlotNumber: 0x0002
  - ModuleIdentNumber: 0x00000032 (Digital IO Module)
  - ModuleProperties: 0x0000
  - NumberOfSubmodules: 0x0001
    - SubslotNumber: 0x0001
    - SubmoduleIdentNumber: 0x00000132
    - SubmoduleProperties: 0x0003 (Input and Output)
    
    Input Descriptor:
    - DataDescription: 0x0001 (Input)
    - DataLength: 0x0001 (1 byte)
    - LengthIOCS: 0x01
    - LengthIOPS: 0x01
    
    Output Descriptor:
    - DataDescription: 0x0002 (Output)
    - DataLength: 0x0001 (1 byte)
    - LengthIOCS: 0x01
    - LengthIOPS: 0x01
```

#### 1.3 等待Connect响应

**超时时间**：2000ms

**响应包结构**：
```
DCE RPC Header:
  - PDU Type: 2 (Response)
  
RPC Body:
  - Return Value: 0x00000000 (成功)
  或其他错误码
```

**错误处理**：
- 返回值 ≠ 0：连接失败，进入Error状态
- 超时：连接失败，停止通信

**成功后**：状态转换为 `ArState::Parameterizing`

---

### 阶段 2：参数化 (Parameterization)

**状态**：`ArState::Parameterizing`

**目的**：通知从站参数传输完成，可以应用配置。

#### 2.1 发送ParamEnd请求 (RPC Control)

**传输协议**：UDP over IPv4

**RPC参数**：
- **Opnum**：0x0004 (Control)
- **Sequence Number**：101

**RPC Body结构**：

```
NDR Header (20 bytes):
  - Maximum: 512
  - Args Length: 根据Block计算
  - Maximum Count: 同上
  - Offset: 0
  - Actual Count: 同上

Block: ControlRequest (Type 0x0110):
  - BlockType: 0x0110
  - BlockLength: 0x001C (28 bytes)
  - BlockVersionHigh: 0x01
  - BlockVersionLow: 0x00
  - Reserved: 0x0000 (2 bytes)
  - ARUUID: 16字节（与Connect中使用的相同）
  - SessionKey: 0x0001
  - AlarmSequenceNumber: 0x0000
  - ControlCommand: 0x0001 (ParamEnd)
  - ControlBlockProperties: 0x0000
```

#### 2.2 等待ParamEnd响应

**超时时间**：2000ms

**响应验证**：
```cpp
int res = waitForResponse(0x0001);
if (res == 0) {
    // ParamEnd accepted
    setState(ArState::AppReady);
} else {
    // ParamEnd rejected
    stop();
}
```

**成功后**：状态转换为 `ArState::AppReady`

---

### 阶段 3：应用就绪 (Application Ready)

**状态**：`ArState::AppReady`

**目的**：等待从站发送ApplicationReady请求，确认从站已准备好进行数据交换。

#### 3.1 等待从站的ApplicationReady请求

**超时时间**：5000ms

**从站发送的RPC请求**：
```
DCE RPC Header:
  - PDU Type: 0 (Request)
  - Opnum: 0x0004 (Control)
  
RPC Body:
  - Block Type: 0x0110 (ControlRequest)
  - Control Command: 0x0002 (ApplicationReady)
```

#### 3.2 主站发送ApplicationReady响应

**响应包结构**：

```
Ethernet Header:
  - Destination MAC: 从站MAC
  - Source MAC: 主站MAC
  - EtherType: 0x0800 (IPv4，注意：不带VLAN标签)

IP Header:
  - 源IP和目标IP互换

UDP Header:
  - 源端口和目标端口互换

DCE RPC Header:
  - PDU Type: 2 (Response)
  - 其他字段复制自请求

RPC Body:
  PNIOStatus (4 bytes):
    - 0x00000000 (Success)
  
  NDR Header (16 bytes):
    - Args Length: 0x00000020 (32 bytes - Block数据长度)
    - Maximum Count: 0x00000020
    - Offset: 0x00000000
    - Actual Count: 0x00000020
  
  Block: ControlBlockRes (Type 0x8112):
    - BlockType: 0x8112 (PF_BT_APPRDY_RES)
    - BlockLength: 0x001C (28 bytes)
    - BlockVersionHigh: 0x01
    - BlockVersionLow: 0x00
    - Reserved: 0x0000 (2 bytes)
    - ARUUID: 16字节（从请求中复制）
    - SessionKey: 2字节（从请求中复制，或默认0x0001）
    - AlarmSequenceNumber: 0x0000
    - ControlCommand: 0x0008 (PF_CONTROL_COMMAND_BIT_DONE)
    - ControlBlockProperties: 0x0000
```

**关键点**：
1. 响应必须包含完整的PROFINET Block结构
2. PNIOStatus必须在NDR Header之前
3. ARUUID和SessionKey应从请求中复制
4. ControlCommand设置为DONE (0x0008)

#### 3.3 启动周期性数据交换

**成功后**：
```cpp
startCyclicExchange();
setState(ArState::Running);
```

**周期定时器**：
```cpp
m_cyclicTimer->start(8);  // 8ms周期（匹配SendClockFactor × ReductionRatio）
```

---

### 阶段 4：周期性数据交换 (Cyclic Data Exchange)

**状态**：`ArState::Running`

**目的**：主站和从站以固定周期交换实时IO数据。

#### 4.1 主站发送周期帧 (Output Data)

**调用频率**：每8ms一次（由`m_cyclicTimer`触发）

**数据包结构**：

```
Ethernet Header (14 bytes):
  - Destination MAC: 从站MAC
  - Source MAC: 主站MAC
  - EtherType: 0x8892 (PROFINET RT - 不带VLAN标签)

PROFINET RT Frame:
  FrameID (2 bytes): 0x8002 (Master Output to Slave)
  
  IO Data (根据IOCR DataLength):
    - Output Data: 1 byte (m_outputData)
    - IOPS: 1 byte (0x80 = GOOD)
    - IOCS: 1 byte (0x80 = GOOD)
  
  APDU Status (4 bytes):
    - Cycle Counter High: (cycleCounter >> 8) & 0xFF
    - Cycle Counter Low: cycleCounter & 0xFF
    - Data Status: 0x35
      * Bit 0 (State): 0 = Backup, 1 = Primary ✓
      * Bit 2 (DataValid): 1 = Valid ✓
      * Bit 4 (Run): 1 = Run ✓
      * Bit 5 (StopMode): 0
    - Transfer Status: 0x00

Padding: 填充到64字节（以太网最小帧长）
```

**周期计数器**：
```cpp
static uint16_t cycleCounter = 0;
cycleCounter += 256;  // 每个周期增加256（高字节变化）
```

**关键参数对应关系**：

| IOCR参数 | 周期帧字段 | 说明 |
|----------|------------|------|
| DataLength = 3 | Data(1) + IOPS(1) + IOCS(1) | 总数据长度 |
| FrameID = 0x8002 | FrameID字段 | 主站输出帧 |
| FrameOffset = 0 | Data起始偏移 | 数据从第0字节开始 |
| FrameOffset = 2 (IOCS) | IOCS偏移 | 在Data+IOPS之后 |

#### 4.2 接收从站周期帧 (Input Data)

**从站发送频率**：每8ms一次

**数据包结构**：

```
Ethernet Header (14 bytes):
  - Destination MAC: 主站MAC
  - Source MAC: 从站MAC
  - EtherType: 0x8892 (可能带VLAN标签0x8100)

VLAN Tag (可选，4 bytes):
  - Tag Protocol ID: 0x8100
  - Priority + VLAN ID: 0xC000 (Priority 6, VLAN 0)
  - Inner EtherType: 0x8892

PROFINET RT Frame:
  FrameID (2 bytes): 0x8001 (Slave Output to Master)
  
  IO Data:
    - Input Data: 1 byte
    - IOPS: 1 byte (0x80 = GOOD)
    - IOCS: 1 byte (0x80 = GOOD)
  
  APDU Status (4 bytes):
    - Cycle Counter: 从站计数器
    - Data Status: 应包含Valid + Run标志
    - Transfer Status: 0x00
```

**主站处理逻辑**：

```cpp
// waitForResponse(0x8001, 0, 5) 持续监听
if (frameId == 0x8001) {
    uint8_t newInputData = data[ethHeaderLen + 2];
    if (newInputData != m_inputData) {
        m_inputData = newInputData;
        emit inputDataReceived(m_inputData);
    }
}
```

**超时检测**：
```cpp
if (res == -2) {  // Timeout
    m_consecutiveTimeouts++;
    if (m_consecutiveTimeouts > 100) {  // ~1秒无数据
        emit messageLogged("Error: Cyclic data exchange timeout.");
        stop();
    }
}
```

#### 4.3 VLAN标签处理

**主站处理**：
```cpp
int ethHeaderLen = 14;
if (type == 0x8100 && header->caplen >= 18) {  // VLAN tagged
    type = (data[16] << 8) | data[17];          // 读取内层EtherType
    ethHeaderLen = 18;                           // 调整偏移量
}
```

**关键点**：
- 主站发送的周期帧**不带VLAN标签**（EtherType直接0x8892）
- 从站可能发送**带VLAN标签**的帧
- 接收时需动态检测并调整偏移量

#### 4.4 ARP处理

**从站的ARP请求**：
- 从站启动后会发送ARP请求，查询主站IP（192.168.0.254）

**主站响应**：
```cpp
if (type == 0x0806) {  // ARP
    if (op == 1) {     // Request
        if (targetIP == 192.168.0.254) {
            sendArpResponse(src, senderIP);
        }
    }
}
```

**ARP响应包结构**：
```
Ethernet Header:
  - Destination MAC: 从站MAC
  - Source MAC: 主站MAC
  - EtherType: 0x0806 (ARP)

ARP:
  - Hardware Type: 0x0001 (Ethernet)
  - Protocol Type: 0x0800 (IPv4)
  - HLEN: 6
  - PLEN: 4
  - Operation: 0x0002 (Reply)
  - Sender MAC: 主站MAC
  - Sender IP: 192.168.0.254
  - Target MAC: 从站MAC
  - Target IP: 从站IP
```

---

## 三、关键数据结构要求

### 3.1 AR (Application Relation) 参数

| 参数 | 值 | 说明 |
|------|-----|------|
| ARUUID | 16 bytes | 唯一标识此AR，整个通信过程保持一致 |
| SessionKey | 0x0001 | 会话密钥 |
| ARType | 0x0001 | IO Controller AR |
| ARProperties | 0x00000211 | 属性标志 |
| CMInitiatorActivityTimeoutFactor | 100 | 超时因子（×100ms = 10秒） |
| CMInitiatorUDPRTPort | 0x8894 | UDP端口34964 |

### 3.2 IOCR (IO Connection Relation) 参数

**Output CR (主站→从站)**：

| 参数 | 值 | 说明 |
|------|-----|------|
| IOCRType | 0x0002 | Output |
| IOCRReference | 0x0002 | 引用编号 |
| FrameID | 0x8002 | 帧标识符 |
| DataLength | 0x0003 | 数据总长度（字节） |
| SendClockFactor | 0x0010 | 16 |
| ReductionRatio | 0x0010 | 16 |
| 实际周期 | 8ms | 16 × 16 × 31.25μs |
| IOCRProperties | 0x00000002 | RT_CLASS_2 |
| WatchdogFactor | 3 | 看门狗因子 |
| DataHoldFactor | 3 | 数据保持因子 |

**Input CR (从站→主站)**：

| 参数 | 值 | 说明 |
|------|-----|------|
| IOCRType | 0x0001 | Input |
| IOCRReference | 0x0001 | 引用编号 |
| FrameID | 0x8001 | 帧标识符 |
| 其他参数 | 同Output | |

### 3.3 Submodule 配置

| 参数 | 值 | 说明 |
|------|-----|------|
| API | 0x00000000 | 应用程序接口 |
| SlotNumber | 0x0002 | 插槽号 |
| SubslotNumber | 0x0001 | 子插槽号 |
| ModuleIdentNumber | 0x00000032 | 模块标识（Digital IO） |
| SubmoduleIdentNumber | 0x00000132 | 子模块标识 |
| Input DataLength | 1 byte | 输入数据长度 |
| Output DataLength | 1 byte | 输出数据长度 |
| IOPS Length | 1 byte | 提供者状态长度 |
| IOCS Length | 1 byte | 消费者状态长度 |

### 3.4 FrameOffset 计算

**Output Frame (0x8002) 布局**：
```
Offset 0: Output Data (1 byte)      ← FrameOffset = 0
Offset 1: IOPS (1 byte)
Offset 2: IOCS (1 byte)             ← FrameOffset = 2 (Data + IOPS)
Offset 3-6: APDU Status (4 bytes)
```

**Input Frame (0x8001) 布局**：
```
Offset 0: Input Data (1 byte)       ← FrameOffset = 0
Offset 1: IOPS (1 byte)
Offset 2: IOCS (1 byte)             ← FrameOffset = 2
Offset 3-6: APDU Status (4 bytes)
```

**关键要求**：
- IOCR中的`DataLength`必须等于实际帧中的数据总长度（不含APDU Status）
- `FrameOffset`必须与实际帧中的偏移量精确对应
- IOCS的`FrameOffset`= 数据长度 + IOPS长度

---

## 四、时序和超时参数

### 4.1 配置阶段超时

| 操作 | 超时时间 | 失败处理 |
|------|----------|----------|
| DCP Set Name | 2000ms | 返回Error |
| DCP Set IP | 1000ms | 返回Error |

### 4.2 连接阶段超时

| 操作 | 超时时间 | 失败处理 |
|------|----------|----------|
| Connect Request | 2000ms | 停止连接 |
| ParamEnd Request | 2000ms | 停止连接 |
| ApplicationReady Wait | 5000ms | 停止连接 |

### 4.3 运行阶段超时

| 参数 | 值 | 计算 |
|------|-----|------|
| 周期性数据检测间隔 | 10ms | phaseTimer间隔 |
| 单次检测超时 | 5ms | waitForResponse |
| 连续超时阈值 | 100次 | ~1秒 |
| AR活动超时 | 10秒 | TimeoutFactor(100) × 100ms |

### 4.4 周期计算

```
发送周期 = SendClockFactor × ReductionRatio × 31.25μs
         = 16 × 16 × 31.25μs
         = 8000μs
         = 8ms
```

---

## 五、错误处理和诊断

### 5.1 常见错误码

| 错误码 | 说明 | 原因 |
|--------|------|------|
| 0x00000000 | Success | 操作成功 |
| -1 | General Failure | pcap句柄无效或通用错误 |
| -2 | Timeout | 等待响应超时 |
| -4 | RPC Fault | RPC协议错误 |

### 5.2 诊断点

**Connect阶段**：
```cpp
// 检查Connect响应的返回值
int res = waitForResponse(0x0000);
if (res >= 0) {
    emit messageLogged("Phase 1: Connect response received.");
} else {
    emit messageLogged("Phase 1: Connect timeout / failed.");
}
```

**周期数据阶段**：
```cpp
// 检查Data Status
uint8_t dataStatus = frame[ethHeaderLen + 5];
bool isValid = (dataStatus & 0x04) != 0;      // Bit 2
bool isRunning = (dataStatus & 0x10) != 0;    // Bit 4
bool isPrimary = (dataStatus & 0x01) != 0;    // Bit 0

if (!isValid) {
    emit messageLogged("Warning: Data not valid from slave");
}
```

### 5.3 日志建议

**关键日志点**：
1. 每阶段状态转换
2. RPC请求和响应的返回值
3. 周期帧的前5次发送（验证格式）
4. 每100次周期数据接收（监控连续性）
5. 所有超时和错误事件

---

## 六、PROFINET协议细节

### 6.1 DCE RPC UUID

| UUID | 用途 |
|------|------|
| DEA00001-6C97-11D1-8271-00A02442DF7D | PROFINET CM Interface |
| DEA00000-6C97-11D1-8271-00A02442DF7D | CM Initiator Object |

### 6.2 Block类型

| BlockType | 名称 | 方向 | 用途 |
|-----------|------|------|------|
| 0x0101 | ARBlockReq | Master→Slave | AR建立请求 |
| 0x0102 | IOCRBlockReq | Master→Slave | IOCR定义 |
| 0x0103 | AlarmCRBlockReq | Master→Slave | Alarm CR定义 |
| 0x0104 | ExpectedSubmoduleBlockReq | Master→Slave | 期望的子模块配置 |
| 0x0110 | ControlBlockReq | Both | 控制命令（ParamEnd/AppReady） |
| 0x8112 | ControlBlockRes | Master→Slave | ApplicationReady响应 |

### 6.3 Control Commands

| Command | 值 | 说明 |
|---------|-----|------|
| ParamEnd | 0x0001 | 参数传输结束 |
| ApplicationReady | 0x0002 | 应用准备就绪 |
| Release | 0x0003 | 释放AR |
| Done | 0x0008 | ApplicationReady确认 |

### 6.4 EtherType

| EtherType | 协议 | 用途 |
|-----------|------|------|
| 0x0800 | IPv4 | RPC通信 |
| 0x0806 | ARP | 地址解析 |
| 0x8100 | 802.1Q | VLAN标签 |
| 0x8892 | PROFINET RT | 实时数据 |

### 6.5 FrameID

| FrameID | 方向 | 用途 |
|---------|------|------|
| 0x8001 | Slave→Master | 输入数据（从站发送） |
| 0x8002 | Master→Slave | 输出数据（主站发送） |
| 0xFEFC | Both | DCP Request |
| 0xFEFD | Both | DCP Response |

---

## 七、实现要点总结

### 7.1 必须遵守的规则

1. **ARUUID一致性**：从Connect到整个通信过程，ARUUID必须保持不变
2. **FrameOffset精确性**：IOCR中声明的偏移量必须与实际帧结构完全匹配
3. **DataLength正确性**：必须等于 实际数据 + IOPS + IOCS 的总长度
4. **VLAN处理**：主站发送RT帧**不带**VLAN标签；接收时需**支持**VLAN标签
5. **APDU Status**：必须包含有效的Cycle Counter和Data Status
6. **ApplicationReady响应**：必须包含完整的Block结构（PNIOStatus + NDR + Block）
7. **周期精度**：周期定时器应尽量接近计算值（8ms）
8. **超时监控**：必须实现连续超时检测，防止僵尸连接

### 7.2 调试建议

1. **使用Wireshark**：
   - 过滤器：`profinet || arp || (udp.port == 34964)`
   - 检查每个阶段的请求和响应
   - 验证FrameID、DataLength、FrameOffset

2. **日志策略**：
   - 初期：记录所有数据包
   - 稳定后：只记录前5次周期帧 + 每100次采样

3. **分阶段测试**：
   - 阶段1：验证DCP配置
   - 阶段2：验证Connect成功
   - 阶段3：验证ParamEnd成功
   - 阶段4：验证ApplicationReady交互
   - 阶段5：验证周期数据交换

---

## 八、参考代码片段

### 8.1 主状态机

```cpp
switch (m_state) {
    case ArState::Connecting:
        sendRpcConnect();
        waitForResponse(0x0000, 0, 2000);
        setState(ArState::Parameterizing);
        break;
        
    case ArState::Parameterizing:
        sendRpcControlParamEnd();
        waitForResponse(0x0001, 0, 2000);
        setState(ArState::AppReady);
        break;
        
    case ArState::AppReady:
        waitForResponse(0x0002, 0, 5000);  // Wait for AppReady request
        // Send ApplicationReady response
        startCyclicExchange();
        setState(ArState::Running);
        break;
        
    case ArState::Running:
        waitForResponse(0x8001, 0, 5);    // Poll for input data
        break;
}
```

### 8.2 周期数据发送

```cpp
void sendCyclicFrame() {
    uint8_t packet[64] = {0};
    
    // Ethernet Header
    memcpy(packet, targetMac, 6);
    memcpy(packet + 6, sourceMac, 6);
    packet[12] = 0x88; packet[13] = 0x92;  // 不带VLAN
    
    // FrameID
    packet[14] = 0x80; packet[15] = 0x02;
    
    // Data
    packet[16] = m_outputData;
    packet[17] = 0x80;  // IOPS Good
    packet[18] = 0x80;  // IOCS Good
    
    // APDU Status
    packet[19] = (cycleCounter >> 8);
    packet[20] = cycleCounter & 0xFF;
    packet[21] = 0x35;  // Valid + Run + Primary
    packet[22] = 0x00;
    
    pcap_sendpacket(handle, packet, 64);
}
```

### 8.3 周期数据接收

```cpp
// 处理VLAN
int ethHeaderLen = 14;
if (etherType == 0x8100) {
    etherType = (data[16] << 8) | data[17];
    ethHeaderLen = 18;
}

// 检查FrameID
if (etherType == 0x8892) {
    uint16_t frameId = (data[ethHeaderLen] << 8) | data[ethHeaderLen + 1];
    
    if (frameId == 0x8001) {
        uint8_t inputData = data[ethHeaderLen + 2];
        uint8_t iops = data[ethHeaderLen + 3];
        uint8_t iocs = data[ethHeaderLen + 4];
        uint8_t dataStatus = data[ethHeaderLen + 5];
        
        if ((dataStatus & 0x04) == 0) {
            qDebug() << "Warning: data_valid == false";
        }
    }
}
```

---

## 九、常见问题排查

### 问题1：Connect超时
**可能原因**：
- 从站IP未正确配置
- UDP端口不匹配
- 防火墙阻止
- ARUUID格式错误

**检查**：
- Wireshark查看是否发送了Connect请求
- 检查从站是否回复Response
- 验证Response的返回值

### 问题2：ParamEnd被拒绝
**可能原因**：
- ARUUID不匹配
- Block结构错误
- SessionKey不匹配

**检查**：
- 对比Connect和ParamEnd中的ARUUID
- 验证ControlCommand = 0x0001

### 问题3：ApplicationReady超时
**可能原因**：
- 从站未发送ApplicationReady请求
- 主站响应格式错误
- Block Type错误（应为0x8112）

**检查**：
- 确认从站日志是否显示"Sending ApplicationReady"
- 验证主站响应的BlockType和ControlCommand

### 问题4：周期数据无效（data_valid == false）
**可能原因**：
- FrameID错误
- DataLength与实际不符
- FrameOffset错误

**检查**：
- 验证FrameID是否为0x8002
- 计算DataLength = Data + IOPS + IOCS
- 检查APDU Status的Data Status字段

### 问题5：Wrong outputdata length
**可能原因**：
- 从站收到的帧长度与IOCR声明不符
- VLAN标签处理错误
- FrameOffset计算错误

**检查**：
- 确认主站发送的帧不带VLAN标签（EtherType直接0x8892）
- 验证从站IOCR配置中的DataLength
- 计算实际Data区域长度

---

## 十、附录

### A. 完整数据包示例

**Connect Request (简化)**：
```
Ethernet: [Dest MAC] [Src MAC] 08 00
IP: 45 00 ... C0 A8 00 FE [Target IP]
UDP: 88 94 88 94 [Length] 00 00
RPC: 04 00 23 00 00 00 00 00 [UUID] ...
Body: [NDR] [Block 0x0101] [Block 0x0102] [Block 0x0102] [Block 0x0103] [Block 0x0104]
```

**Cyclic Output Frame**：
```
[Dest MAC] [Src MAC] 88 92    // Ethernet
80 02                          // FrameID
[Data] 80 80                   // Data + IOPS + IOCS
[CC_H] [CC_L] 35 00           // APDU Status
```

**Cyclic Input Frame (带VLAN)**：
```
[Dest MAC] [Src MAC] 81 00    // Ethernet + VLAN tag
C0 00 88 92                    // Priority 6, VLAN 0, EtherType 0x8892
80 01                          // FrameID
[Data] 80 80                   // Data + IOPS + IOCS
[CC_H] [CC_L] 35 00           // APDU Status
```

### B. 关键宏定义（p-net从站）

```c
#define PNET_MAX_SESSION_BUFFER_SIZE        4096
#define PF_FRAME_ID_IOC_0                   0x8000
#define PF_FRAME_ID_IOC_1                   0x8001
#define PF_FRAME_ID_IOC_2                   0x8002
#define PF_CONTROL_COMMAND_PRM_END          0x0001
#define PF_CONTROL_COMMAND_APP_RDY          0x0002
#define PF_CONTROL_COMMAND_BIT_DONE         0x0008
#define PF_BT_AR_BLOCK_REQ                  0x0101
#define PF_BT_IOC_BLOCK_REQ                 0x0102
#define PF_BT_ALARM_BLOCK_REQ               0x0103
#define PF_BT_EXPECTED_SUBMODULE_BLOCK_REQ  0x0104
#define PF_BT_CONTROL_BLOCK_REQ             0x0110
#define PF_BT_APPRDY_RES                    0x8112
```

---

**文档版本**：1.0  
**最后更新**：2026-01-03  
**适用项目**：PNConfigGenerator / ArExchangeManager
