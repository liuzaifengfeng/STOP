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

export type ToolPage = 'dashboard' | 'pairing' | 'configuration' | 'radio' | 'firmware' | 'instructions' | 'settings'
export type DeviceRole = 'controller' | 'receiver'
export type RadioEntry = { time: string; direction: 'RX' | 'TX'; value: string }
type PendingOta = OtaAttempt & { awaitingReboot: boolean }

export function useDeviceTool(role: DeviceRole) {
  const client = new StopDeviceClient(role === 'controller' ? 'STOP-C6' : 'STOP-C6-RX')
  const expectedProductId = role === 'controller' ? 1 : 2
  const expectedProject = role === 'controller' ? 'STOP_TX' : 'STOP_RX'
  const otaPendingKey = () => `stop-c6.pending-ota.v2.${role}.${info.value?.bluetoothMac ?? 'unknown'}`
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
  const firmwareProject = ref('')
  const firmwareUrl = ref('')
  const ota = reactive<OtaProgress>({ sent: 0, total: 0, percent: 0, state: 0 })
  const linkMetrics = ref<LinkMetrics>(client.getLinkMetrics())
  let linkProbeTimer: number | undefined
  let linkProbeRunning = false

  const unsubscribe = client.onFrame((frame) => {
    linkMetrics.value = client.getLinkMetrics()
    if (frame.type === MessageType.Error && frame.payload.length === 0) {
      stopLinkProbe()
      connected.value = false
      status.value = undefined
      notice.value = '设备已断开'
      return
    }
    if (frame.type === MessageType.DeviceStatusEvent && frame.payload.length >= 20) {
      status.value = parseDeviceStatus(frame.payload)
    }
    if (frame.type === MessageType.RadioRxEvent && (frame.flags & FrameFlag.Event) && frame.payload.length >= 2) {
      const length = new DataView(frame.payload.buffer, frame.payload.byteOffset).getUint16(0, true)
      if (2 + length > frame.payload.length) return
      const data = frame.payload.subarray(2, 2 + length)
      radioLog.value.unshift({ time: new Date().toLocaleTimeString(), direction: 'RX', value: `${bytesToHex(data)}  ·  ${safeText(data)}` })
      radioLog.value.length = Math.min(radioLog.value.length, 100)
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
      info.value = undefined
      status.value = undefined
      try {
        await client.connect()
        connected.value = true
        info.value = await client.getInfo()
        if (info.value.productId !== expectedProductId) {
          throw new Error(`设备产品编号 ${info.value.productId} 与当前${role === 'controller' ? '控制端' : '被控端'}位置不符`)
        }
        status.value = await client.getStatus()
        Object.assign(config, await client.getConfig())
        if (info.value && (info.value.capabilities & Capability.OtaResult) !== 0) await reconcileOtaResult(await client.getOtaResult())
        linkMetrics.value = client.getLinkMetrics()
        startLinkProbe()
      } catch (error) {
        client.disconnect()
        connected.value = false
        status.value = undefined
        throw error
      }
    })
  }

  function disconnect() {
    stopLinkProbe()
    client.disconnect()
    connected.value = false
    status.value = undefined
    notice.value = '已主动断开'
  }

  function startLinkProbe() {
    stopLinkProbe()
    linkProbeTimer = window.setInterval(async () => {
      if (!connected.value || busy.value || linkProbeRunning) return
      linkProbeRunning = true
      try { status.value = await client.getStatus() } catch { /* 请求统计由客户端记录。 */ }
      finally { linkMetrics.value = client.getLinkMetrics(); linkProbeRunning = false }
    }, 5000)
  }

  function stopLinkProbe() {
    if (linkProbeTimer !== undefined) window.clearInterval(linkProbeTimer)
    linkProbeTimer = undefined
  }

  async function refresh() {
    const value = await run('刷新状态', () => client.getStatus())
    if (value) status.value = value
    return value !== undefined
  }

  async function debugOutput(enable: boolean) {
    if (role !== 'receiver' || !connected.value) { notice.value = '请先连接被控端'; return false }
    const result = await run(enable ? '临时调试使能' : '断开输出', async () => {
      await client.debugOutput(enable)
      return true
    })
    if (!result) {
      const failure = notice.value
      await refresh()
      notice.value = failure + '；旧固件请先升级，安全拒绝请查看本页条件提示及串口电源日志'
      return false
    }
    if (!await refresh()) { notice.value = '命令已应答，但状态读回失败，请检查实际输出'; return false }
    const on = Boolean((status.value?.pairFlags ?? 0) & 16)
    notice.value = on === enable ? (on ? '实际输出已接通' : '实际输出已断开') : '实际输出与请求不一致，检查串口安全日志'
    return on === enable
  }

  async function startPairing() {
    if (role !== 'receiver' || !connected.value) { notice.value = '请先连接被控端'; return false }
    if (!await refresh()) return false
    if (status.value?.safetyState === 3) { notice.value = '被控端输出已接通，不能进入配对模式'; return false }
    const result = await run('开启 30 秒配对窗口', async () => { await client.startPairing(); return true })
    if (!result) return false
    await refresh()
    return true
  }

  async function startRadioListen() {
    if (role !== 'controller' || !connected.value) { notice.value = '请先连接控制端'; return false }
    return (await run('开启控制端 10 秒接收窗口', async () => {
      await client.startRadioListen()
      return true
    })) === true
  }

  async function saveConfig() {
    const value = await run('保存并回读参数', async () => {
      const requested = { ...config }
      await client.setConfig(requested)
      const actual = await client.getConfig()
      for (const key of Object.keys(requested) as (keyof DeviceConfig)[]) {
        if (actual[key] !== requested[key]) throw new Error('参数回读不一致，请重新读取设备配置')
      }
      return actual
    })
    if (value) {
      Object.assign(config, value)
      notice.value = value.radioLink === undefined ? '参数已保存并回读确认' : '参数已保存并回读确认；无线模式和信道在设备重启后生效，请配置两端后分别重新上电'
    }
  }

  async function sendRadio() {
    let data: Uint8Array
    try { data = radioMode.value === 'hex' ? hexToBytes(radioInput.value) : textBytes(radioInput.value) }
    catch (error) { notice.value = error instanceof Error ? error.message : String(error); return false }
    const sent = await run('发送无线诊断包', () => client.radioSend(data))
    if (sent !== undefined) radioLog.value.unshift({ time: new Date().toLocaleTimeString(), direction: 'TX', value: `${bytesToHex(data)}  ·  ${safeText(data)}` })
    return sent !== undefined
  }

  async function sendDiagnostic(data: Uint8Array) {
    const sent = await run('发送联调测试包', () => client.radioSend(data))
    if (sent) radioLog.value.unshift({ time: new Date().toLocaleTimeString(), direction: 'TX', value: `${bytesToHex(data)}  ·  ${safeText(data)}` })
    return sent !== undefined
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
    firmware.value = undefined
    firmwareName.value = name
    firmwareVersion.value = ''
    firmwareProject.value = ''
    if (image.length < 112) { notice.value = '固件文件太短，无法读取 ESP 应用描述'; return }
    const view = new DataView(image.buffer, image.byteOffset, image.byteLength)
    if (view.getUint32(32, true) !== 0xabcd5432) { notice.value = '未找到 ESP 应用描述，请确认选择的是 app firmware.bin'; return }
    const versionBytes = image.subarray(48, 80)
    const projectBytes = image.subarray(80, 112)
    const end = versionBytes.indexOf(0)
    const projectEnd = projectBytes.indexOf(0)
    firmwareVersion.value = bytesText(end >= 0 ? versionBytes.subarray(0, end) : versionBytes).trim()
    firmwareProject.value = bytesText(projectEnd >= 0 ? projectBytes.subarray(0, projectEnd) : projectBytes).trim()
    if (firmwareProject.value !== expectedProject) {
      notice.value = `固件项目 ${firmwareProject.value || '未知'} 不属于当前${role === 'controller' ? '控制端' : '被控端'}（应为 ${expectedProject}）`
      return
    }
    firmware.value = image
    ota.total = image.length
    notice.value = `已读取固件版本 ${firmwareVersion.value}`
  }

  async function startOta() {
    if (!firmware.value) { notice.value = '请先选择或下载固件'; return }
    if (!info.value || info.value.productId !== expectedProductId || firmwareProject.value !== expectedProject) {
      notice.value = '设备身份或固件项目不匹配，已拒绝升级'; return
    }
    if (role === 'receiver' && status.value?.safetyState === 3) {
      notice.value = '被控端输出已接通，请先在硬件上断开输出后再升级'; return
    }
    Object.assign(ota, { sent: 0, percent: 0, state: 1 })
    busy.value = true
    notice.value = '执行 BLE OTA'
    try {
      const attempt = await client.otaUpdate(firmware.value, firmwareVersion.value,
        info.value.productId, info.value.hardwareRevision, (progress) => Object.assign(ota, progress),
        (prepared) => savePendingOta({ ...prepared, awaitingReboot: false }))
      savePendingOta({ ...attempt, awaitingReboot: true })
      notice.value = '固件写入完成，请等待设备重启后重新连接以确认结果'
    } catch (error) {
      if (ota.state !== 4) localStorage.removeItem(otaPendingKey())
      notice.value = error instanceof Error ? error.message : String(error)
    } finally { busy.value = false }
  }

  function savePendingOta(value: PendingOta) { localStorage.setItem(otaPendingKey(), JSON.stringify(value)) }
  function loadPendingOta(): PendingOta | undefined {
    try {
      const value = JSON.parse(localStorage.getItem(otaPendingKey()) ?? 'null') as Partial<PendingOta> | null
      if (!value || typeof value.transferId !== 'number' || typeof value.imageSize !== 'number' || typeof value.sha256 !== 'string' || typeof value.version !== 'string') return undefined
      return value as PendingOta
    } catch { return undefined }
  }

  async function reconcileOtaResult(result: OtaResult) {
    const pending = loadPendingOta()
    if (!pending) return
    if (result.state === 0 && pending.awaitingReboot && info.value?.firmwareVersion === pending.version) {
      window.alert(`设备已运行目标版本 ${pending.version}\n首次迁移无法完成跨重启 SHA-256 确认，后续升级将自动确认。`)
      localStorage.removeItem(otaPendingKey())
      Object.assign(ota, { sent: 0, total: 0, percent: 0, state: 0 })
      return
    }
    if (result.transferId !== pending.transferId || result.imageSize !== pending.imageSize || result.sha256.toLowerCase() !== pending.sha256.toLowerCase()) return
    const shortHash = result.sha256.slice(0, 12)
    if (result.state === 2) {
      window.alert(`OTA 升级成功\n版本：${pending.version}\nSHA-256：${shortHash}…`)
      localStorage.removeItem(otaPendingKey())
      Object.assign(ota, { sent: 0, total: 0, percent: 0, state: 0 })
    } else if (result.state === 3) {
      window.alert(`OTA 升级失败或已回滚\n错误码：0x${result.error.toString(16).padStart(4, '0')}\nSHA-256：${shortHash}…`)
      localStorage.removeItem(otaPendingKey())
      Object.assign(ota, { sent: 0, total: pending.imageSize, percent: 0, state: 5 })
    } else if (result.state === 1) notice.value = '新固件已经启动，但关键服务运行确认尚未完成'
  }

  return {
    role, expectedProject, activePage, connected, busy, notice, info, status, config, radioMode, radioInput, radioLog,
    firmware, firmwareName, firmwareVersion, firmwareProject, firmwareUrl, ota, batteryText, signalStrength,
    linkQuality, otaStateText, deviceName: computed(() => client.name), connect, disconnect,
    refresh, debugOutput, startPairing, startRadioListen, saveConfig, sendRadio, sendDiagnostic, chooseFirmware, loadFirmwareUrl, startOta,
  }
}
