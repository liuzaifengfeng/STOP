import {
  bytesText, crc32, decodeFrame, encodeFrame, FrameFlag, MessageType,
  parseDeviceInfo, parseDeviceStatus, textBytes, UUID,
  type DeviceInfo, type DeviceStatus, type StopFrame,
} from './protocol'

export interface DeviceConfig {
  alias: string
  statusPeriodMs: number
  radioTxTimeoutMs: number
}

export interface OtaProgress {
  sent: number
  total: number
  percent: number
  state: number
}

export interface OtaAttempt {
  transferId: number
  imageSize: number
  sha256: string
  version: string
}

export interface OtaResult {
  state: number
  error: number
  transferId: number
  imageSize: number
  sha256: string
}

export interface LinkMetrics {
  samples: number
  successes: number
  failures: number
  averageRttMs: number | null
  failureRate: number
}

type Pending = { resolve: (frame: StopFrame) => void; reject: (error: Error) => void; timer: number; startedAt: number }
type LinkSample = { success: boolean; rttMs?: number }
type FrameListener = (frame: StopFrame) => void

export class StopDeviceClient {
  private device?: BluetoothDevice
  private control?: BluetoothRemoteGATTCharacteristic
  private bulk?: BluetoothRemoteGATTCharacteristic
  private pending = new Map<number, Pending>()
  private listeners = new Set<FrameListener>()
  private requestId = 0
  private sequence = 0
  private linkSamples: LinkSample[] = []

  get connected(): boolean { return this.device?.gatt?.connected === true }
  get name(): string { return this.device?.name ?? 'STOP-C6' }
  getLinkMetrics(): LinkMetrics {
    const successes = this.linkSamples.filter((sample) => sample.success)
    const failures = this.linkSamples.length - successes.length
    const rttTotal = successes.reduce((sum, sample) => sum + (sample.rttMs ?? 0), 0)
    return {
      samples: this.linkSamples.length,
      successes: successes.length,
      failures,
      averageRttMs: successes.length ? rttTotal / successes.length : null,
      failureRate: this.linkSamples.length ? failures / this.linkSamples.length : 0,
    }
  }

  async connect(): Promise<void> {
    if (!navigator.bluetooth) throw new Error('当前浏览器不支持 Web Bluetooth，请使用 Chrome 或 Edge')
    this.device = await navigator.bluetooth.requestDevice({ filters: [{ namePrefix: 'STOP-C6' }], optionalServices: [UUID.service] })
    this.device.addEventListener('gattserverdisconnected', this.handleDisconnect)
    const server = await this.device.gatt?.connect()
    if (!server) throw new Error('无法创建 BLE GATT 连接')
    const service = await server.getPrimaryService(UUID.service)
    const [control, bulk, event, status, bulkAck] = await Promise.all([
      service.getCharacteristic(UUID.control), service.getCharacteristic(UUID.bulk), service.getCharacteristic(UUID.event),
      service.getCharacteristic(UUID.status), service.getCharacteristic(UUID.bulkAck),
    ])
    this.control = control
    this.bulk = bulk
    for (const characteristic of [event, status, bulkAck]) {
      characteristic.addEventListener('characteristicvaluechanged', this.handleNotification)
      await characteristic.startNotifications()
    }
  }

  disconnect(): void { this.device?.gatt?.disconnect() }
  onFrame(listener: FrameListener): () => void { this.listeners.add(listener); return () => this.listeners.delete(listener) }

  private handleDisconnect = (): void => {
    this.control = undefined
    this.bulk = undefined
    for (const pending of this.pending.values()) {
      window.clearTimeout(pending.timer)
      this.recordLinkSample(false)
      pending.reject(new Error('BLE 连接已断开'))
    }
    this.pending.clear()
    this.listeners.forEach((listener) => listener({ type: MessageType.Error, flags: FrameFlag.Event, requestId: 0, sequence: 0, payload: new Uint8Array() }))
  }

  private handleNotification = (event: Event): void => {
    const characteristic = event.target as BluetoothRemoteGATTCharacteristic
    if (!characteristic.value) return
    try {
      const frame = decodeFrame(characteristic.value)
      const pending = this.pending.get(frame.requestId)
      if (pending && frame.requestId !== 0 && (frame.flags & FrameFlag.Response)) {
        window.clearTimeout(pending.timer)
        this.pending.delete(frame.requestId)
        this.recordLinkSample(true, performance.now() - pending.startedAt)
        if (frame.flags & FrameFlag.Error) {
          const errorOffset = frame.type === MessageType.OtaStatus ? 1 : 0
          const code = frame.payload.length >= errorOffset + 2
            ? new DataView(frame.payload.buffer, frame.payload.byteOffset).getUint16(errorOffset, true)
            : -1
          pending.reject(new Error(`设备拒绝请求，错误码 0x${code.toString(16).padStart(4, '0')}`))
        } else pending.resolve(frame)
      }
      this.listeners.forEach((listener) => listener(frame))
    } catch (error) {
      console.error('忽略损坏的 BLE 通知', error)
    }
  }

  private nextRequestId(): number { this.requestId = (this.requestId % 0xffff) + 1; return this.requestId }
  private nextSequence(): number { this.sequence = (this.sequence + 1) >>> 0; return this.sequence }

  private recordLinkSample(success: boolean, rttMs?: number): void {
    this.linkSamples.push({ success, rttMs })
    if (this.linkSamples.length > 50) this.linkSamples.shift()
  }

  private waitForResponse(requestId: number, timeoutMs: number): Promise<StopFrame> {
    return new Promise((resolve, reject) => {
      const startedAt = performance.now()
      const timer = window.setTimeout(() => {
        if (this.pending.delete(requestId)) this.recordLinkSample(false)
        reject(new Error('设备响应超时'))
      }, timeoutMs)
      this.pending.set(requestId, { resolve, reject, timer, startedAt })
    })
  }

  private async request(type: MessageType, payload = new Uint8Array(), timeoutMs = 8000): Promise<StopFrame> {
    if (!this.control || !this.connected) throw new Error('设备尚未连接')
    const requestId = this.nextRequestId()
    const response = this.waitForResponse(requestId, timeoutMs)
    try {
      await this.control.writeValueWithResponse(encodeFrame(type, FrameFlag.Request, requestId, this.nextSequence(), payload) as BufferSource)
      return await response
    } catch (error) {
      const pending = this.pending.get(requestId)
      if (pending) {
        window.clearTimeout(pending.timer)
        this.recordLinkSample(false)
      }
      this.pending.delete(requestId)
      throw error
    }
  }

  private async bulkRequest(payload: Uint8Array): Promise<StopFrame> {
    if (!this.bulk || !this.connected) throw new Error('设备尚未连接')
    const requestId = this.nextRequestId()
    const response = this.waitForResponse(requestId, 10000)
    try {
      await this.bulk.writeValueWithoutResponse(encodeFrame(MessageType.OtaData, FrameFlag.Request, requestId, this.nextSequence(), payload) as BufferSource)
      return await response
    } catch (error) {
      const pending = this.pending.get(requestId)
      if (pending) {
        window.clearTimeout(pending.timer)
        this.recordLinkSample(false)
      }
      this.pending.delete(requestId)
      throw error
    }
  }

  async getInfo(): Promise<DeviceInfo> { return parseDeviceInfo((await this.request(MessageType.DeviceInfoGet)).payload) }
  async getStatus(): Promise<DeviceStatus> { return parseDeviceStatus((await this.request(MessageType.DeviceStatusGet)).payload) }

  async getOtaResult(): Promise<OtaResult> {
    return parseOtaResult((await this.request(MessageType.OtaResult)).payload)
  }

  async getConfig(): Promise<DeviceConfig> {
    const frame = await this.request(MessageType.ConfigGet, new Uint8Array([0]))
    return parseConfig(frame.payload)
  }

  async setConfig(config: DeviceConfig): Promise<DeviceConfig> {
    const alias = textBytes(config.alias)
    const payload = new Uint8Array(1 + 5 + alias.length + 9 + 9)
    const view = new DataView(payload.buffer)
    let offset = 0
    payload[offset++] = 3
    view.setUint16(offset, 1, true); payload[offset + 2] = 3; view.setUint16(offset + 3, alias.length, true); payload.set(alias, offset + 5); offset += 5 + alias.length
    view.setUint16(offset, 2, true); payload[offset + 2] = 2; view.setUint16(offset + 3, 4, true); view.setUint32(offset + 5, config.statusPeriodMs, true); offset += 9
    view.setUint16(offset, 3, true); payload[offset + 2] = 2; view.setUint16(offset + 3, 4, true); view.setUint32(offset + 5, config.radioTxTimeoutMs, true)
    return parseConfig((await this.request(MessageType.ConfigSet, payload)).payload)
  }

  async radioSend(data: Uint8Array): Promise<number> {
    if (data.length === 0 || data.length > 235) throw new Error('E22 数据长度必须为 1～235 字节')
    const payload = new Uint8Array(2 + data.length)
    new DataView(payload.buffer).setUint16(0, data.length, true)
    payload.set(data, 2)
    const response = await this.request(MessageType.RadioSend, payload)
    return new DataView(response.payload.buffer, response.payload.byteOffset).getUint16(0, true)
  }

  async otaUpdate(
    image: Uint8Array,
    version: string,
    onProgress: (progress: OtaProgress) => void,
    onAttempt?: (attempt: OtaAttempt) => void,
  ): Promise<OtaAttempt> {
    if (!version || textBytes(version).length > 31) throw new Error('固件版本必须为 1～31 字节')
    const sha = new Uint8Array(await crypto.subtle.digest('SHA-256', image as BufferSource))
    const versionBytes = textBytes(version)
    const transferId = crypto.getRandomValues(new Uint32Array(1))[0] || 1
    const attempt = { transferId, imageSize: image.length, sha256: compactHex(sha), version }
    onAttempt?.(attempt)
    const begin = new Uint8Array(15 + versionBytes.length + 34)
    const beginView = new DataView(begin.buffer)
    beginView.setUint32(0, transferId, true)
    beginView.setUint32(4, image.length, true)
    beginView.setUint16(8, 478, true)
    beginView.setUint16(10, 1, true)
    beginView.setUint16(12, 1, true)
    begin[14] = versionBytes.length
    begin.set(versionBytes, 15)
    begin.set(sha, 15 + versionBytes.length)
    beginView.setUint16(47 + versionBytes.length, 0, true)
    const beginStatus = parseOtaStatus((await this.request(MessageType.OtaBegin, begin, 20000)).payload)
    if (beginStatus.chunkSize === 0) throw new Error('设备未返回有效 OTA 分块大小')

    let offset = beginStatus.offset
    while (offset < image.length) {
      const chunk = image.subarray(offset, Math.min(offset + beginStatus.chunkSize, image.length))
      const data = new Uint8Array(14 + chunk.length)
      const view = new DataView(data.buffer)
      view.setUint32(0, transferId, true); view.setUint32(4, offset, true); view.setUint16(8, chunk.length, true)
      data.set(chunk, 10); view.setUint32(10 + chunk.length, crc32(chunk), true)
      const status = parseOtaStatus((await this.bulkRequest(data)).payload)
      offset = status.offset
      onProgress({ sent: offset, total: image.length, percent: Math.round(offset * 100 / image.length), state: status.state })
    }
    const end = new Uint8Array(4)
    new DataView(end.buffer).setUint32(0, transferId, true)
    const finalStatus = parseOtaStatus((await this.request(MessageType.OtaEnd, end, 30000)).payload)
    onProgress({ sent: image.length, total: image.length, percent: 100, state: finalStatus.state })
    if (finalStatus.state !== 4) throw new Error(`设备未进入重启状态（OTA 状态 ${finalStatus.state}）`)
    return attempt
  }
}

function parseConfig(payload: Uint8Array): DeviceConfig {
  if (payload.length < 1) throw new Error('参数响应为空')
  const result: Partial<DeviceConfig> = {}
  const view = new DataView(payload.buffer, payload.byteOffset, payload.byteLength)
  let offset = 1
  for (let index = 0; index < payload[0]; index += 1) {
    if (offset + 5 > payload.length) throw new Error('参数 TLV 损坏')
    const key = view.getUint16(offset, true), type = payload[offset + 2], length = view.getUint16(offset + 3, true)
    offset += 5
    if (offset + length > payload.length) throw new Error('参数 TLV 长度错误')
    if (key === 1 && type === 3) result.alias = bytesText(payload.subarray(offset, offset + length))
    else if (key === 2 && type === 2 && length === 4) result.statusPeriodMs = view.getUint32(offset, true)
    else if (key === 3 && type === 2 && length === 4) result.radioTxTimeoutMs = view.getUint32(offset, true)
    offset += length
  }
  if (result.alias === undefined || result.statusPeriodMs === undefined || result.radioTxTimeoutMs === undefined) throw new Error('设备缺少必要参数')
  return result as DeviceConfig
}

function parseOtaStatus(payload: Uint8Array): { state: number; error: number; offset: number; chunkSize: number } {
  if (payload.length !== 18) throw new Error('OTA 状态长度错误')
  const view = new DataView(payload.buffer, payload.byteOffset, payload.byteLength)
  return { state: payload[0], error: view.getUint16(1, true), offset: view.getUint32(7, true), chunkSize: view.getUint16(15, true) }
}

function parseOtaResult(payload: Uint8Array): OtaResult {
  if (payload.length !== 43) throw new Error('OTA 结果长度错误')
  const view = new DataView(payload.buffer, payload.byteOffset, payload.byteLength)
  return {
    state: payload[0],
    error: view.getUint16(1, true),
    transferId: view.getUint32(3, true),
    imageSize: view.getUint32(7, true),
    sha256: compactHex(payload.subarray(11, 43)),
  }
}

function compactHex(value: Uint8Array): string {
  return [...value].map((byte) => byte.toString(16).padStart(2, '0')).join('')
}
