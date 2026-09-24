# STOP-C6 BLE 设备管理协议

## 1. 状态与目标

本文档定义无线急停项目的 BLE 设备管理协议第 1 版。该协议用于云端网页、Android/iOS App、桌面工具或现场网关与 ESP32-C6 之间的维护通信。

当前阶段已经实现 BLE/GATT、STOP 帧、设备信息与状态、基础参数、E22 诊断收发和开发级 OTA。安全认证、量产固件签名、完整安全状态机和断路器联锁尚未实现。协议后续扩展必须遵守 `SAFETY_TARGET.md`：BLE 属于维护管理平面，不得成为安全控制链路，不得解除急停或强制供电。

## 2. 分层模型

```text
业务服务
  设备信息 / 参数 / E22诊断 / OTA / 日志
                    ↓
STOP帧协议
  版本 / 类型 / 请求ID / 序号 / 长度 / 校验
                    ↓
BLE传输层
  CONTROL / BULK_DATA / EVENT / STATUS / BULK_ACK
                    ↓
客户端适配器
  Web Bluetooth / Android BLE / iOS CoreBluetooth / PC网关
```

设备固件只依赖 GATT 和 STOP 帧协议。客户端所使用的 UI 框架和 BLE API 不得改变 UUID、字节序、消息语义、错误码或 OTA 状态机。

## 3. BLE 角色与连接约束

- ESP32-C6：BLE Peripheral、GATT Server。
- 网页、手机或网关：BLE Central、GATT Client。
- 第 1 版同时只允许一个维护客户端连接。
- 设备广播名：`STOP-C6`。
- 广播包含完整 128 位设备管理服务 UUID。
- 所有多字节整数均使用小端序。
- 客户端连接后应订阅 `EVENT`、`STATUS` 和 `BULK_ACK`，再开始事务。
- 客户端不得假定固定 ATT MTU。单次写入必须小于等于当前链路允许的属性值长度。
- 当前传输层单次接收上限为 512 字节；业务层应将大数据拆分为多个帧。
- 客户端开始事务前应等待 MTU 协商完成。当前包含负载的标准响应通常要求 MTU 不低于 41；建议 Android 请求 517，其他平台使用系统协商值。

## 4. GATT 定义

### 4.1 Device Management Service

服务 UUID：

```text
7f610000-9f79-4b5e-8a62-67c6a4b82d10
```

| 名称 | UUID | 属性 | 方向 | 用途 |
|---|---|---|---|---|
| `CONTROL` | `7f610001-9f79-4b5e-8a62-67c6a4b82d10` | Write / Write Without Response | 客户端到设备 | 短命令、请求和小型参数数据 |
| `BULK_DATA` | `7f610002-9f79-4b5e-8a62-67c6a4b82d10` | Write / Write Without Response | 客户端到设备 | OTA 固件块或其他批量数据 |
| `EVENT` | `7f610003-9f79-4b5e-8a62-67c6a4b82d10` | Notify | 设备到客户端 | 异步事件、E22 收包和日志 |
| `STATUS` | `7f610004-9f79-4b5e-8a62-67c6a4b82d10` | Read / Notify | 设备到客户端 | 当前状态摘要和状态变化 |
| `BULK_ACK` | `7f610005-9f79-4b5e-8a62-67c6a4b82d10` | Notify | 设备到客户端 | 批量传输偏移、窗口确认和错误 |

第 1 版同时保留带响应写入和无响应写入：

- 控制命令默认使用带响应写入，确保 GATT 层已经接收。
- OTA 数据可在事务建立后使用无响应写入提高吞吐，但必须依赖 `BULK_ACK` 做应用层流控。
- GATT 写入成功只表示传输层已经接收或排队，不表示业务操作已经成功。

## 5. STOP 帧格式

### 5.1 通用帧

每个特征值承载一个完整 STOP 帧，不允许一个 STOP 帧跨多个 GATT Write 或 Notify。大数据由业务层拆成多帧。

| 偏移 | 长度 | 字段 | 说明 |
|---:|---:|---|---|
| 0 | 2 | `magic` | 固定为 ASCII `ST`，即 `0x53 0x54` |
| 2 | 1 | `version` | 当前为 `0x01` |
| 3 | 1 | `message_type` | 消息类型 |
| 4 | 1 | `flags` | 请求、响应、事件、ACK、错误标志 |
| 5 | 1 | `header_length` | 当前固定为 16 |
| 6 | 2 | `request_id` | 请求与响应关联；异步事件可为 0 |
| 8 | 4 | `sequence` | 发送方单调递增序号，连接重建后可重新起始 |
| 12 | 2 | `payload_length` | 负载长度，最大 492 字节 |
| 14 | 2 | `header_crc16` | CRC-16/CCITT-FALSE，覆盖字节 0..13 |
| 16 | N | `payload` | 消息负载 |
| 16+N | 4 | `frame_crc32` | CRC-32/ISO-HDLC，覆盖头部和负载 |

最短帧长度为 20 字节，最大帧长度为 512 字节。

CRC 用于发现随机传输和实现错误，不等于安全认证。后续安全认证应在会话层或业务负载中增加随机数、会话 ID 和消息认证码，不能改变 CRC 的用途。

### 5.2 flags

| 位 | 名称 | 说明 |
|---:|---|---|
| 0 | `REQUEST` | 请求 |
| 1 | `RESPONSE` | 对请求的最终响应 |
| 2 | `EVENT` | 无请求触发的异步事件 |
| 3 | `ACK` | 中间确认或流控确认 |
| 4 | `ERROR` | 响应负载包含错误信息 |
| 5..7 | 保留 | 必须发送 0，接收端忽略未知保留位 |

`REQUEST`、`RESPONSE` 和 `EVENT` 正常情况下互斥。`ACK` 或 `ERROR` 可以与 `RESPONSE`/`EVENT` 组合。

## 6. 消息类型

| 值 | 名称 | 通道 | 当前阶段 |
|---:|---|---|---|
| `0x01` | `DEVICE_INFO_GET` | CONTROL | 已实现 |
| `0x02` | `DEVICE_STATUS_GET` | CONTROL | 已实现 |
| `0x03` | `DEVICE_STATUS_EVENT` | STATUS | 已实现 |
| `0x10` | `CONFIG_GET` | CONTROL | 已实现 |
| `0x11` | `CONFIG_SET` | CONTROL | 已实现 |
| `0x12` | `CONFIG_RESULT` | EVENT | 已实现 |
| `0x20` | `RADIO_SEND` | CONTROL | 已实现（诊断数据） |
| `0x21` | `RADIO_SEND_RESULT` | EVENT | 已实现 |
| `0x22` | `RADIO_RX_EVENT` | EVENT | 已实现 |
| `0x30` | `OTA_BEGIN` | CONTROL | 已实现（开发级安全） |
| `0x31` | `OTA_DATA` | BULK_DATA | 已实现 |
| `0x32` | `OTA_QUERY` | CONTROL | 已实现（同次开机续传） |
| `0x33` | `OTA_END` | CONTROL | 已实现 |
| `0x34` | `OTA_ABORT` | CONTROL | 已实现 |
| `0x35` | `OTA_STATUS` | BULK_ACK / EVENT | 已实现 |
| `0x36` | `OTA_RESULT` | CONTROL | 已实现（跨重启结果查询） |
| `0x40` | `LOG_EVENT` | EVENT | 预留业务实现 |
| `0x7F` | `ERROR` | EVENT / RESPONSE | 预留业务实现 |

未知消息类型应返回 `UNSUPPORTED_MESSAGE`，不得导致断言、重启或修改设备状态。

## 7. 请求、响应与事件

### 7.1 请求事务

1. 客户端为每个并发请求分配非零 `request_id`。
2. 客户端在 `CONTROL` 写入带 `REQUEST` 标志的帧。
3. 设备完成格式检查和排队后，GATT 写入返回成功。
4. 业务服务通过 `EVENT` 返回具有相同 `request_id` 的 `RESPONSE`。
5. 超时由客户端按消息类型处理；超时后不得直接假定设备没有执行操作，应先查询状态。

第 1 版建议客户端只保持一个正在执行的修改类事务；只读请求最多并发 4 个。

### 7.3 DEVICE_INFO_GET 响应

负载为：

```text
protocol_version uint8
capabilities     uint32
product_id       uint16
hardware_rev     uint16
version_length   uint8
firmware_version UTF-8[version_length]
bluetooth_mac    byte[6]
```

能力位：bit0 基础参数、bit1 E22 诊断、bit2 BLE OTA、bit3 SHA-256、bit4 已启用设备端签名验证、bit5 支持跨重启 OTA 结果查询、bit6 支持连接 RSSI。当前普通开发构建不会设置 bit4。

### 7.2 异步事件

异步事件使用 `request_id=0` 和 `EVENT` 标志。客户端应依据 `message_type` 分发，不得依赖事件到达顺序推断安全状态。

## 8. OTA 交互目标

BLE 传输层只负责可靠地把帧交给 OTA 服务。OTA 服务后续按以下状态机实现：

```text
IDLE -> PREPARING -> RECEIVING -> VERIFYING -> READY_TO_REBOOT
  \         \            \            \
   +----------+------------+-------------> FAILED / ABORTED
```

### 8.1 OTA_BEGIN

建议负载包含：

```text
transfer_id    uint32
image_size     uint32
chunk_size     uint16
product_id     uint16
hardware_rev   uint16
version_length uint8
version        UTF-8
sha256         32 bytes
signature_len  uint16
signature      byte[signature_len]
```

当前按钮盒标识为 `product_id=0x0001`，首版硬件为 `hardware_rev=0x0001`。`version_length` 范围为 1..31。当前实现尚未配置应用层签名公钥，因此 `signature_len` 必须为 0；若启用 ESP-IDF Secure Boot，则 ESP 镜像自身的签名仍会在 `esp_ota_end()` 中验证。未启用 Secure Boot 的构建只能提供传输完整性，不能抵御恶意固件，禁止作为量产安全升级方案。

设备验证产品类型、硬件版本、镜像大小、电池状态、安全断电联锁和签名策略后，使用 `OTA_STATUS` 返回允许的块大小以及当前可靠写入偏移。传输结束时还会校验 ESP 镜像内嵌的 `project_name` 必须与当前固件一致、内嵌版本必须与 `version` 字段一致，以防误刷其他工程镜像；这只能防误操作，不能代替数字签名。

### 8.2 OTA_DATA

建议负载包含：

```text
transfer_id uint32
offset      uint32
data_length uint16
data        byte[data_length]
data_crc32  uint32
```

- `offset` 必须等于设备当前期望偏移，否则设备通过 `BULK_ACK` 返回正确偏移。
- 设备只确认已经写入 OTA 分区的数据。
- 客户端使用有限窗口发送，不得无限制连续 Write Without Response。
- 断线重连后，客户端发送 `OTA_QUERY`，从设备确认的偏移续传。
- `accepted_chunk_size` 会限制为 `min(requested_chunk, 478, ATT_MTU-37)`，客户端必须使用设备返回值，不能继续使用请求值。

当前实现支持同一次开机期间的 BLE 断线重连与偏移查询，不支持设备掉电后的断点续传。设备掉电后必须重新发送 `OTA_BEGIN` 并从偏移 0 开始。

### 8.3 OTA_END

设备应依次完成镜像完整性检查、SHA-256、产品与硬件匹配、数字签名验证，再设置启动分区。网页端校验不能替代设备端校验。

`OTA_END` 的负载固定为 `transfer_id uint32`。设备验证成功并返回 `READY_TO_REBOOT` 后约 1.5 秒自动重启。启用回滚配置时，新固件只有在 NVS、ADC、E22 与 BLE 关键服务启动成功后才确认有效。

接收控制盒进入 OTA 前必须处于锁存断电并确认输出反馈；升级结束或回滚后仍需本地人工复位。

### 8.4 OTA_STATUS

`OTA_STATUS` 固定为 18 字节：

```text
state               uint8
last_error          uint16
transfer_id         uint32
expected_offset     uint32
image_size          uint32
accepted_chunk_size uint16
reboot_pending      uint8
```

### 8.5 OTA_RESULT

客户端在设备重启并重新连接后发送无负载的 `OTA_RESULT (0x36)` 请求。设备返回固定 43 字节：

```text
result_state        uint8     0=无记录，1=等待运行确认，2=成功，3=失败或回滚
last_error          uint16
transfer_id         uint32
image_size          uint32
image_sha256        32 bytes
```

设备仅在完整固件通过块 CRC、整包 SHA-256、ESP 镜像与产品/版本检查后，才将 `transfer_id`、镜像大小、SHA-256、目标分区和“等待运行确认”状态写入 NVS。新固件启动后，只有 NVS、ADC、E22 和 BLE 等关键服务启动成功且 OTA 回滚确认完成，才把记录改为“成功”。若后续启动回到非目标分区，设备把该记录改为“失败或回滚”。

网页必须在开始传输时把本次 `transfer_id`、镜像大小和 SHA-256 保存在本地。重连查询时三者必须全部相符，才能把设备记录判定为本次网页发起的升级结果；不得仅凭版本号或进度达到 100% 宣告成功。结果为成功或失败后，网页应提示用户并清除本地等待记录及 100% 进度状态。

从不支持 `OTA_RESULT` 的旧固件首次升级到支持该功能的版本时，旧固件无法提前写入交接记录。网页可在新固件报告目标版本已经运行后清除等待界面，但必须明确提示“首次迁移无法完成跨重启 SHA-256 确认”，不得把它显示为已验证成功。

状态值依次为 `IDLE=0`、`PREPARING=1`、`RECEIVING=2`、`VERIFYING=3`、`READY_TO_REBOOT=4`、`FAILED=5`、`ABORTED=6`。

## 8.6 基础参数协议

参数采用 TLV 列表。列表首字节为条目数，每个条目为：

```text
key    uint16
type   uint8
length uint16
value  byte[length]
```

`CONFIG_GET` 负载为 `count uint8` 后跟 `count` 个 `key uint16`；`count=0` 读取全部参数。`CONFIG_SET` 使用完整 TLV 列表，设备先验证全部条目，再写入 NVS 并提交。响应统一使用 `CONFIG_RESULT` 的完整 TLV 列表。

| key | 名称 | 类型 | 范围/默认值 |
|---:|---|---|---|
| `0x0001` | `device_alias` | UTF-8 字符串 | 1..31 字节，默认 `STOP-C6` |
| `0x0002` | `status_period_ms` | `uint32` | 1000..60000，默认 5000 |
| `0x0003` | `radio_tx_timeout_ms` | `uint32` | 100..5000，默认 1000 |

`device_alias` 是业务层显示名称；为保持扫描与配对行为稳定，它不会动态修改 GATT GAP 广播名 `STOP-C6`。

### 8.7 设备状态与 E22 诊断

`DEVICE_STATUS_EVENT` 基础负载为 20 字节；支持能力位 bit6 的设备扩展为 21 字节。客户端必须接受尾部扩展字段：

| 偏移 | 字段 | 类型 |
|---:|---|---|
| 0 | `uptime_seconds` | uint32 |
| 4 | `battery_mv` | uint16 |
| 6 | `battery_soc` | uint8，未知为 255 |
| 7 | `radio_ready` | uint8 |
| 8 | `radio_mode` | uint8 |
| 9 | `ble_connected` | uint8 |
| 10 | `att_mtu` | uint16 |
| 12 | `ota_state` | uint8 |
| 13 | `ota_last_error` | uint16 |
| 15 | `ota_expected_offset` | uint32 |
| 19 | `safety_state` | uint8 |
| 20 | `ble_rssi_dbm` | int8，控制器暂时无法测量时为 127 |

安全状态在 `safety_manager` 实现前固定为 0，不得解释为允许供电。

RSSI 只表示 C6 接收网页端信号的瞬时强弱，不等同于链路质量。网页的链路质量评分综合最近 50 次应用层请求的响应/超时情况、平均往返延迟和 RSSI；样本不足时显示“采样中”。BLE 链路层会自动重传，当前控制器接口未向应用提供逐包重传计数，因此页面所示失败率是应用层 ACK 超时/失败率，不得标称为真实射频丢包率。网页空闲时可每 5 秒探测一次，OTA 和其他操作期间应暂停额外探测。

`RADIO_SEND` 负载为 `data_length uint16 + data`，长度不得超过 235 字节；成功响应返回实际发送长度。`RADIO_RX_EVENT` 使用相同负载格式。该通道只用于诊断报文，未来安全接收端必须在协议层将其与急停安全报文隔离。

## 9. 错误码

统一错误负载建议为：

```text
error_code    uint16
detail_length uint8
detail        UTF-8, optional
```

| 值 | 名称 | 含义 |
|---:|---|---|
| `0x0000` | `OK` | 成功 |
| `0x0001` | `INVALID_FRAME` | 帧格式错误 |
| `0x0002` | `UNSUPPORTED_VERSION` | 协议版本不支持 |
| `0x0003` | `UNSUPPORTED_MESSAGE` | 消息类型不支持 |
| `0x0004` | `INVALID_ARGUMENT` | 参数非法 |
| `0x0005` | `NOT_AUTHENTICATED` | 会话未认证 |
| `0x0006` | `NOT_AUTHORIZED` | 当前身份无权限 |
| `0x0007` | `BUSY` | 资源或事务忙 |
| `0x0008` | `INVALID_STATE` | 当前设备状态不允许操作 |
| `0x0009` | `QUEUE_FULL` | 接收队列已满 |
| `0x000A` | `TIMEOUT` | 操作超时 |
| `0x000B` | `CRC_ERROR` | 数据校验失败 |
| `0x000C` | `OFFSET_MISMATCH` | 批量数据偏移错误 |
| `0x000D` | `IMAGE_REJECTED` | 固件产品、硬件或签名检查失败 |
| `0x000E` | `SAFETY_LOCKOUT` | 安全联锁禁止操作 |
| `0x00FF` | `INTERNAL_ERROR` | 未分类内部错误 |

## 10. 安全与权限演进

第 1 阶段传输层不把 BLE 连接等同于可信身份。后续至少加入：

- 物理维护模式或短时配对窗口。
- 每设备唯一密钥或证书。
- 随机数挑战、会话 ID、防重放计数器和消息认证码。
- 只读、维护、升级等权限级别。
- 失败次数限制和认证审计记录。
- 固件设备端签名验证。

认证失败不得影响急停安全控制平面，但可断开 BLE 维护连接或暂时停止广播。

## 11. 客户端实现约束

### 11.1 Web Bluetooth

- 页面必须通过 HTTPS 提供。
- `requestDevice()` 必须由用户点击触发。
- 使用 `ArrayBuffer`/`DataView` 编解码，禁止把 OTA 数据转换为 JSON 或 Base64。
- 页面切后台或系统休眠可能导致断连，OTA 必须支持查询偏移和续传。

### 11.2 Android/iOS App

- Android 使用标准 BLE GATT API，iOS 使用 CoreBluetooth。
- App 与网页共用同一帧编解码测试向量。
- 原生 App 可以优化重连、缓存固件和后台行为，但不得定义私有设备命令。

## 12. 兼容性规则

- 同一主版本内只能追加消息类型、字段尾部或能力位，不得改变既有字段含义。
- 接收端必须依据长度跳过未知可选字段。
- 无法安全忽略的协议变化必须提升 `version`。
- 客户端连接后先读取设备能力与协议版本，再启用对应功能。
- UUID 在产品生命周期内保持稳定。

## 13. 当前实现边界

当前 C6 固件已实现：

- NimBLE Peripheral 初始化与广播。
- 上述五个 GATT 特征。
- CONTROL/BULK_DATA 写入排队。
- EVENT/STATUS/BULK_ACK 发送接口。
- 单连接状态、订阅状态和协商 MTU 记录。
- 断线后恢复广播。
- STOP 帧解析、CRC16/CRC32 与统一错误响应。
- 设备信息、电池/E22/OTA 状态读取和周期通知。
- 三项基础参数的范围验证、NVS 持久化和完整结果回读。
- E22 诊断收发桥接。
- OTA 备用分区写入、分块确认、偏移查询、块 CRC、纯软件增量 SHA-256、ESP 镜像校验、启动分区切换、回滚确认，以及通过 NVS 保存并跨重启查询升级结果。软件 SHA 避免特定 ESP32-C6/ESP-IDF 组合的硬件 SHA DMA 上下文异常。

当前阶段仍未实现：

- BLE 身份认证与绑定。
- 设备掉电后的 OTA 断点续传。
- 独立的应用层固件签名公钥与签名校验。
- E22 模块寄存器参数的在线修改（当前只提供应用数据诊断收发）。
- `safety_manager`、物理维护模式、断路器反馈与 OTA 硬件联锁。
- 通过 BLE 解除急停或控制危险输出。
