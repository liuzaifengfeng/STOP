import { computed, onUnmounted, reactive, ref } from 'vue'
import {
  StopDeviceClient,
  type DeviceConfig,
  type LinkMetrics,
  type OtaAttempt,
  type OtaProgress,
  type OtaResult,
} from '../device-client'
import {
  bytesText,
  bytesToHex,
  Capability,
  FrameFlag,
  hexToBytes,
  MessageType,
  parseDeviceStatus,
  textBytes,
  type DeviceInfo,
  type DeviceStatus,
} from '../protocol'

export type ToolPage = 'dashboard' | 'configuration' | 'radio' | 'firmware' | 'settings'
export type RadioEntry = { time: string; direction: 'RX' | 'TX'; value: string }
type PendingOta = OtaAttempt & { awaitingReboot: boolean }

const OTA_PENDING_KEY = 'stop-c6.pending-ota.v1'
const client = new StopDeviceClient()

export function useDeviceTool() {
  const activePage = ref<ToolPage>('dashboard')
  const connected = ref(false)
  const busy = ref(false)
  const notice = ref('等待连接设备')
  const info = ref<DeviceInfo>()
  const status = ref<DeviceStatus>()
  const config = reactive<DeviceConfig>({ alias: 'STOP-C6', statusPeriodMs: 5000, radioTxTimeoutMs: 1000 })
  const radioMode = ref<'text' | 'hex'>('text')
  const radioInput = ref('')
  const radioLog = ref<RadioEntry[]>([])
  const firmware = ref<Uint8Array>()
  const firmwareName = ref('')
  const firmwareVersion = ref('')
  const firmwareUrl = ref('')
  const ota = reactive<OtaProgress>({ sent: 0, total: 0, percent: 0, state: 0 })
  const linkMetrics = ref<LinkMetrics>(client.getLinkMetrics())
  let linkProbeTimer: number | undefined

  const unsubscribe = client.onFrame((frame) => {
    linkMetrics.value = client.getLinkMetrics()
    if (frame.type === MessageType.Error && frame.payload.length === 0) {
      stopLinkProbe()
      connected.value = false
      notice.value = '设备已断开'
      return
    }
    if (frame.type === MessageType.DeviceStatusEvent && frame.payload.length >= 20) {
      status.value = parseDeviceStatus(frame.payload)
    }
    if (frame.type === MessageType.RadioRxEvent && (frame.flags & FrameFlag.Event) && frame.payload.length >= 2) {
      const length = new DataView(frame.payload.buffer, frame.payload.byteOffset).getUint16(0, true)
      const data = frame.payload.subarray(2, 2 + length)
      radioLog.value.unshift({ time: new Date().toLocaleTimeString(), direction: 'RX', value: `${bytesToHex(data)}  ·  ${safeText(data)}` })
    }
  })

  onUnmounted(() => {
    stopLinkProbe()
    unsubscribe()
  })

  const batteryText = computed(() => status.value?.batterySoc == null ? '—' : `${status.value.batterySoc}%`)
  const signalStrength = computed(() => {
    const rssi = status.value?.rssi
    if (rssi == null) return { label: '测量中', level: 'unknown' }
    if (rssi >= -55) return { label: '很强', level: 'excellent' }
    if (rssi >= -67) return { label: '较强', level: 'good' }
    if (rssi >= -75) return { label: '一般', level: 'fair' }
    return { label: '较弱', level: 'weak' }
  })
  const linkQuality = computed(() => {
    const metrics = linkMetrics.value
    if (metrics.samples < 4) return { label: '采样中', level: 'unknown', score: null as number | null, title: `有效样本 ${metrics.samples}/4` }
    const successScore = (1 - metrics.failureRate) * 100
    const latencyScore = metrics.averageRttMs == null ? 0 : Math.max(0, Math.min(100, (700 - metrics.averageRttMs) / 6))
    const rssi = status.value?.rssi
    const rssiScore = rssi == null ? null : Math.max(0, Math.min(100, (rssi + 100) * 2))
    const score = Math.round(rssiScore == null
      ? successScore * 0.67 + latencyScore * 0.33
      : rssiScore * 0.25 + successScore * 0.5 + latencyScore * 0.25)
    const quality = score >= 85 ? ['优秀', 'excellent'] : score >= 70 ? ['良好', 'good'] : score >= 50 ? ['一般', 'fair'] : ['较差', 'weak']
    const rtt = metrics.averageRttMs == null ? '—' : `${Math.round(metrics.averageRttMs)} ms`
    return { label: quality[0], level: quality[1], score, title: `样本 ${metrics.samples}，平均 RTT ${rtt}，失败率 ${(metrics.failureRate * 100).toFixed(1)}%` }
  })
  const otaStateText = computed(() => ['空闲', '准备', '接收中', '校验中', '等待重启', '失败', '已取消'][ota.state] ?? `状态 ${ota.state}`)

  async function run<T>(label: string, operation: () => Promise<T>): Promise<T | undefined> {
    busy.value = true
    notice.value = label
    try {
      const result = await operation()
      notice.value = `${label}完成`
      return result
    } catch (error) {
      notice.value = error instanceof Error ? error.message : String(error)
      return undefined
    } finally {
      busy.value = false
    }
  }

  async function connect() {
    await run('连接设备', async () => {
      await client.connect()
      connected.value = true
      const results = await Promise.allSettled([client.getInfo(), client.getStatus(), client.getConfig()])
      if (results[0].status === 'fulfilled') info.value = results[0].value
      if (results[1].status === 'fulfilled') status.value = results[1].value
      if (results[2].status === 'fulfilled') Object.assign(config, results[2].value)
      const failures = results.filter((result) => result.status === 'rejected')
      if (failures.length) throw new Error(`设备已连接，但有 ${failures.length} 项初始化读取失败`)
      if (info.value && (info.value.capabilities & Capability.OtaResult) !== 0) await reconcileOtaResult(await client.getOtaResult())
      linkMetrics.value = client.getLinkMetrics()
      startLinkProbe()
    })
  }

  function disconnect() {
    stopLinkProbe()
    client.disconnect()
    connected.value = false
    notice.value = '已主动断开'
  }

  function startLinkProbe() {
    stopLinkProbe()
    linkProbeTimer = window.setInterval(async () => {
      if (!connected.value || busy.value) return
      try { status.value = await client.getStatus() } catch { /* 请求统计由客户端记录。 */ }
      finally { linkMetrics.value = client.getLinkMetrics() }
    }, 5000)
  }

  function stopLinkProbe() {
    if (linkProbeTimer !== undefined) window.clearInterval(linkProbeTimer)
    linkProbeTimer = undefined
  }

  async function refresh() {
    const value = await run('刷新状态', () => client.getStatus())
    if (value) status.value = value
  }

  async function saveConfig() {
    const value = await run('保存参数', () => client.setConfig({ ...config }))
    if (value) Object.assign(config, value)
  }

  async function sendRadio() {
    const data = radioMode.value === 'hex' ? hexToBytes(radioInput.value) : textBytes(radioInput.value)
    const sent = await run('发送 E22 诊断包', () => client.radioSend(data))
    if (sent) radioLog.value.unshift({ time: new Date().toLocaleTimeString(), direction: 'TX', value: `${bytesToHex(data)}  ·  ${safeText(data)}` })
  }

  function safeText(data: Uint8Array) {
    const value = bytesText(data)
    return [...value].every((character) => character >= ' ' && character !== '\u007f') ? value : '[二进制]'
  }

  async function chooseFirmware(event: Event) {
    const file = (event.target as HTMLInputElement).files?.[0]
    if (file) setFirmware(new Uint8Array(await file.arrayBuffer()), file.name)
  }

  async function loadFirmwareUrl() {
    const value = await run('从云端下载固件', async () => {
      const response = await fetch(firmwareUrl.value, { cache: 'no-store' })
      if (!response.ok) throw new Error(`固件下载失败：HTTP ${response.status}`)
      return new Uint8Array(await response.arrayBuffer())
    })
    if (value) setFirmware(value, firmwareUrl.value.split('/').pop() || 'cloud-firmware.bin')
  }

  function setFirmware(image: Uint8Array, name: string) {
    firmware.value = image
    firmwareName.value = name
    ota.total = image.length
    if (image.length < 80) { notice.value = '固件文件太短，无法读取 ESP 应用描述'; return }
    const view = new DataView(image.buffer, image.byteOffset, image.byteLength)
    if (view.getUint32(32, true) !== 0xabcd5432) { notice.value = '未找到 ESP 应用描述，请确认选择的是 app firmware.bin'; return }
    const versionBytes = image.subarray(48, 80)
    const end = versionBytes.indexOf(0)
    firmwareVersion.value = bytesText(end >= 0 ? versionBytes.subarray(0, end) : versionBytes).trim()
    notice.value = `已读取固件版本 ${firmwareVersion.value}`
  }

  async function startOta() {
    if (!firmware.value) { notice.value = '请先选择或下载固件'; return }
    Object.assign(ota, { sent: 0, percent: 0, state: 1 })
    busy.value = true
    notice.value = '执行 BLE OTA'
    try {
      const attempt = await client.otaUpdate(firmware.value, firmwareVersion.value, (progress) => Object.assign(ota, progress),
        (prepared) => savePendingOta({ ...prepared, awaitingReboot: false }))
      savePendingOta({ ...attempt, awaitingReboot: true })
      notice.value = '固件写入完成，请等待设备重启后重新连接以确认结果'
    } catch (error) {
      if (ota.state !== 4) localStorage.removeItem(OTA_PENDING_KEY)
      notice.value = error instanceof Error ? error.message : String(error)
    } finally { busy.value = false }
  }

  function savePendingOta(value: PendingOta) { localStorage.setItem(OTA_PENDING_KEY, JSON.stringify(value)) }
  function loadPendingOta(): PendingOta | undefined {
    try {
      const value = JSON.parse(localStorage.getItem(OTA_PENDING_KEY) ?? 'null') as Partial<PendingOta> | null
      if (!value || typeof value.transferId !== 'number' || typeof value.imageSize !== 'number' || typeof value.sha256 !== 'string' || typeof value.version !== 'string') return undefined
      return value as PendingOta
    } catch { return undefined }
  }

  async function reconcileOtaResult(result: OtaResult) {
    const pending = loadPendingOta()
    if (!pending) return
    if (result.state === 0 && pending.awaitingReboot && info.value?.firmwareVersion === pending.version) {
      window.alert(`设备已运行目标版本 ${pending.version}\n首次迁移无法完成跨重启 SHA-256 确认，后续升级将自动确认。`)
      localStorage.removeItem(OTA_PENDING_KEY)
      Object.assign(ota, { sent: 0, total: 0, percent: 0, state: 0 })
      return
    }
    if (result.transferId !== pending.transferId || result.imageSize !== pending.imageSize || result.sha256.toLowerCase() !== pending.sha256.toLowerCase()) return
    const shortHash = result.sha256.slice(0, 12)
    if (result.state === 2) {
      window.alert(`OTA 升级成功\n版本：${pending.version}\nSHA-256：${shortHash}…`)
      localStorage.removeItem(OTA_PENDING_KEY)
      Object.assign(ota, { sent: 0, total: 0, percent: 0, state: 0 })
    } else if (result.state === 3) {
      window.alert(`OTA 升级失败或已回滚\n错误码：0x${result.error.toString(16).padStart(4, '0')}\nSHA-256：${shortHash}…`)
      localStorage.removeItem(OTA_PENDING_KEY)
      Object.assign(ota, { sent: 0, total: pending.imageSize, percent: 0, state: 5 })
    } else if (result.state === 1) notice.value = '新固件已经启动，但关键服务运行确认尚未完成'
  }

  return {
    activePage, connected, busy, notice, info, status, config, radioMode, radioInput, radioLog,
    firmware, firmwareName, firmwareVersion, firmwareUrl, ota, batteryText, signalStrength,
    linkQuality, otaStateText, deviceName: computed(() => client.name), connect, disconnect,
    refresh, saveConfig, sendRadio, chooseFirmware, loadFirmwareUrl, startOta,
  }
}
