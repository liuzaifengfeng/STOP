export const UUID = {
  service: '7f610000-9f79-4b5e-8a62-67c6a4b82d10',
  control: '7f610001-9f79-4b5e-8a62-67c6a4b82d10',
  bulk: '7f610002-9f79-4b5e-8a62-67c6a4b82d10',
  event: '7f610003-9f79-4b5e-8a62-67c6a4b82d10',
  status: '7f610004-9f79-4b5e-8a62-67c6a4b82d10',
  bulkAck: '7f610005-9f79-4b5e-8a62-67c6a4b82d10',
} as const

export enum MessageType {
  DeviceInfoGet = 0x01,
  DeviceStatusGet = 0x02,
  DeviceStatusEvent = 0x03,
  ConfigGet = 0x10,
  ConfigSet = 0x11,
  ConfigResult = 0x12,
  RadioSend = 0x20,
  RadioSendResult = 0x21,
  RadioRxEvent = 0x22,
  OtaBegin = 0x30,
  OtaData = 0x31,
  OtaQuery = 0x32,
  OtaEnd = 0x33,
  OtaAbort = 0x34,
  OtaStatus = 0x35,
  Error = 0x7f,
}

export enum FrameFlag {
  Request = 1 << 0,
  Response = 1 << 1,
  Event = 1 << 2,
  Ack = 1 << 3,
  Error = 1 << 4,
}

export interface StopFrame {
  type: MessageType
  flags: number
  requestId: number
  sequence: number
  payload: Uint8Array
}

export interface DeviceInfo {
  protocolVersion: number
  capabilities: number
  productId: number
  hardwareRevision: number
  firmwareVersion: string
  bluetoothMac: string
}

export interface DeviceStatus {
  uptimeSeconds: number
  batteryMv: number
  batterySoc: number | null
  radioReady: boolean
  radioMode: number
  bleConnected: boolean
  mtu: number
  otaState: number
  otaError: number
  otaOffset: number
  safetyState: number
}

const decoder = new TextDecoder()
const encoder = new TextEncoder()

export function crc16(data: Uint8Array): number {
  let crc = 0xffff
  for (const byte of data) {
    crc ^= byte << 8
    for (let bit = 0; bit < 8; bit += 1) crc = crc & 0x8000 ? ((crc << 1) ^ 0x1021) & 0xffff : (crc << 1) & 0xffff
  }
  return crc
}

export function crc32(data: Uint8Array): number {
  let crc = 0xffffffff
  for (const byte of data) {
    crc ^= byte
    for (let bit = 0; bit < 8; bit += 1) crc = crc & 1 ? (crc >>> 1) ^ 0xedb88320 : crc >>> 1
  }
  return (crc ^ 0xffffffff) >>> 0
}

export function encodeFrame(type: MessageType, flags: number, requestId: number, sequence: number, payload: Uint8Array = new Uint8Array()): Uint8Array {
  if (payload.length > 492) throw new Error('STOP 帧负载超过 492 字节')
  const result = new Uint8Array(20 + payload.length)
  const view = new DataView(result.buffer)
  result[0] = 0x53
  result[1] = 0x54
  result[2] = 1
  result[3] = type
  result[4] = flags
  result[5] = 16
  view.setUint16(6, requestId, true)
  view.setUint32(8, sequence, true)
  view.setUint16(12, payload.length, true)
  view.setUint16(14, crc16(result.subarray(0, 14)), true)
  result.set(payload, 16)
  view.setUint32(16 + payload.length, crc32(result.subarray(0, 16 + payload.length)), true)
  return result
}

export function decodeFrame(input: DataView | Uint8Array): StopFrame {
  const data = input instanceof Uint8Array ? input : new Uint8Array(input.buffer, input.byteOffset, input.byteLength)
  if (data.length < 20 || data[0] !== 0x53 || data[1] !== 0x54 || data[2] !== 1 || data[5] !== 16) throw new Error('无效 STOP 帧头')
  const view = new DataView(data.buffer, data.byteOffset, data.byteLength)
  const payloadLength = view.getUint16(12, true)
  if (data.length !== 20 + payloadLength) throw new Error('STOP 帧长度不匹配')
  if (view.getUint16(14, true) !== crc16(data.subarray(0, 14))) throw new Error('STOP 帧头 CRC 错误')
  if (view.getUint32(16 + payloadLength, true) !== crc32(data.subarray(0, 16 + payloadLength))) throw new Error('STOP 帧 CRC 错误')
  return {
    type: data[3] as MessageType,
    flags: data[4],
    requestId: view.getUint16(6, true),
    sequence: view.getUint32(8, true),
    payload: data.slice(16, 16 + payloadLength),
  }
}

export function parseDeviceInfo(payload: Uint8Array): DeviceInfo {
  if (payload.length < 16) throw new Error('设备信息长度错误')
  const view = new DataView(payload.buffer, payload.byteOffset, payload.byteLength)
  const versionLength = payload[9]
  if (payload.length !== 16 + versionLength) throw new Error('设备版本长度错误')
  return {
    protocolVersion: payload[0],
    capabilities: view.getUint32(1, true),
    productId: view.getUint16(5, true),
    hardwareRevision: view.getUint16(7, true),
    firmwareVersion: decoder.decode(payload.subarray(10, 10 + versionLength)),
    bluetoothMac: [...payload.subarray(10 + versionLength)].map((value) => value.toString(16).padStart(2, '0')).join(':'),
  }
}

export function parseDeviceStatus(payload: Uint8Array): DeviceStatus {
  if (payload.length !== 20) throw new Error('设备状态长度错误')
  const view = new DataView(payload.buffer, payload.byteOffset, payload.byteLength)
  return {
    uptimeSeconds: view.getUint32(0, true), batteryMv: view.getUint16(4, true), batterySoc: payload[6] === 255 ? null : payload[6],
    radioReady: payload[7] !== 0, radioMode: payload[8], bleConnected: payload[9] !== 0, mtu: view.getUint16(10, true),
    otaState: payload[12], otaError: view.getUint16(13, true), otaOffset: view.getUint32(15, true), safetyState: payload[19],
  }
}

export function textBytes(value: string): Uint8Array { return encoder.encode(value) }
export function bytesText(value: Uint8Array): string { return decoder.decode(value) }

export function hexToBytes(value: string): Uint8Array {
  const clean = value.replace(/0x/gi, '').replace(/[^a-fA-F0-9]/g, '')
  if (clean.length === 0 || clean.length % 2 !== 0) throw new Error('HEX 必须包含偶数个十六进制字符')
  return Uint8Array.from(clean.match(/.{2}/g)!, (part) => Number.parseInt(part, 16))
}

export function bytesToHex(value: Uint8Array): string { return [...value].map((byte) => byte.toString(16).padStart(2, '0')).join(' ') }
