<script setup lang="ts">
import { computed, onUnmounted, reactive, ref } from 'vue'
import { StopDeviceClient, type DeviceConfig, type OtaProgress } from './device-client'
import { bytesText, bytesToHex, FrameFlag, hexToBytes, MessageType, parseDeviceStatus, textBytes, type DeviceInfo, type DeviceStatus } from './protocol'

const client = new StopDeviceClient()
const connected = ref(false), busy = ref(false), notice = ref('等待连接设备')
const info = ref<DeviceInfo>(), status = ref<DeviceStatus>(), config = reactive<DeviceConfig>({ alias: 'STOP-C6', statusPeriodMs: 5000, radioTxTimeoutMs: 1000 })
const radioMode = ref<'text' | 'hex'>('text'), radioInput = ref(''), radioLog = ref<Array<{ time: string; direction: string; value: string }>>([])
const firmware = ref<Uint8Array>(), firmwareName = ref(''), firmwareVersion = ref(''), firmwareUrl = ref('')
const ota = reactive<OtaProgress>({ sent: 0, total: 0, percent: 0, state: 0 })
const unsubscribe = client.onFrame((frame) => {
  if (frame.type === MessageType.Error && frame.payload.length === 0) { connected.value = false; notice.value = '设备已断开'; return }
  if (frame.type === MessageType.DeviceStatusEvent && frame.payload.length === 20) status.value = parseDeviceStatus(frame.payload)
  if (frame.type === MessageType.RadioRxEvent && (frame.flags & FrameFlag.Event) && frame.payload.length >= 2) {
    const length = new DataView(frame.payload.buffer, frame.payload.byteOffset).getUint16(0, true)
    const data = frame.payload.subarray(2, 2 + length)
    radioLog.value.unshift({ time: new Date().toLocaleTimeString(), direction: 'RX', value: `${bytesToHex(data)}  ·  ${safeText(data)}` })
  }
})
onUnmounted(unsubscribe)

const batteryText = computed(() => status.value?.batterySoc == null ? '—' : `${status.value.batterySoc}%`)
const otaStateText = computed(() => ['空闲', '准备', '接收中', '校验中', '等待重启', '失败', '已取消'][ota.state] ?? `状态 ${ota.state}`)

async function run<T>(label: string, operation: () => Promise<T>): Promise<T | undefined> {
  busy.value = true; notice.value = label
  try { const result = await operation(); notice.value = `${label}完成`; return result }
  catch (error) { notice.value = error instanceof Error ? error.message : String(error); return undefined }
  finally { busy.value = false }
}

async function connect() {
  await run('连接设备', async () => {
    await client.connect(); connected.value = true
    const results = await Promise.allSettled([client.getInfo(), client.getStatus(), client.getConfig()])
    if (results[0].status === 'fulfilled') info.value = results[0].value as DeviceInfo
    if (results[1].status === 'fulfilled') status.value = results[1].value as DeviceStatus
    if (results[2].status === 'fulfilled') Object.assign(config, results[2].value as DeviceConfig)
    const failures = results.filter((result) => result.status === 'rejected')
    if (failures.length) throw new Error(`设备已连接，但有 ${failures.length} 项初始化读取失败`)
  })
}
function disconnect() { client.disconnect(); connected.value = false; notice.value = '已主动断开' }
async function refresh() { const value = await run('刷新状态', () => client.getStatus()); if (value) status.value = value }
async function saveConfig() { const value = await run('保存参数', () => client.setConfig({ ...config })); if (value) Object.assign(config, value) }
async function sendRadio() {
  const data = radioMode.value === 'hex' ? hexToBytes(radioInput.value) : textBytes(radioInput.value)
  const sent = await run('发送 E22 诊断包', () => client.radioSend(data))
  if (sent) radioLog.value.unshift({ time: new Date().toLocaleTimeString(), direction: 'TX', value: `${bytesToHex(data)}  ·  ${safeText(data)}` })
}
function safeText(data: Uint8Array): string { const text = bytesText(data); return [...text].every((char) => char >= ' ' && char !== '\u007f') ? text : '[二进制]' }
async function chooseFirmware(event: Event) {
  const file = (event.target as HTMLInputElement).files?.[0]
  if (!file) return
  setFirmware(new Uint8Array(await file.arrayBuffer()), file.name)
}
async function loadFirmwareUrl() {
  const value = await run('从云端下载固件', async () => {
    const response = await fetch(firmwareUrl.value, { cache: 'no-store' }); if (!response.ok) throw new Error(`固件下载失败：HTTP ${response.status}`)
    return new Uint8Array(await response.arrayBuffer())
  })
  if (value) setFirmware(value, firmwareUrl.value.split('/').pop() || 'cloud-firmware.bin')
}
function setFirmware(image: Uint8Array, name: string) {
  firmware.value = image; firmwareName.value = name; ota.total = image.length
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
  ota.sent = 0; ota.percent = 0; ota.state = 1
  await run('执行 BLE OTA', () => client.otaUpdate(firmware.value!, firmwareVersion.value, (progress) => Object.assign(ota, progress)))
}
</script>

<template>
  <div class="shell">
    <header class="topbar">
      <div><span class="eyebrow">LAB SAFETY / MAINTENANCE</span><h1>STOP-C6 工具箱</h1></div>
      <div class="connection"><span :class="['dot', { online: connected }]" />{{ connected ? `已连接 ${client.name}` : '未连接' }}
        <button v-if="!connected" class="primary" :disabled="busy" @click="connect">连接设备</button>
        <button v-else class="ghost" @click="disconnect">断开</button>
      </div>
    </header>

    <div class="safety-banner"><strong>维护通道</strong><span>本工具不得解除急停、复位安全锁存或强制恢复供电。</span></div>
    <p class="notice">{{ notice }}</p>

    <main>
      <section class="hero-grid">
        <article class="card identity"><div class="card-head"><h2>设备概览</h2><button class="icon-button" :disabled="!connected || busy" @click="refresh">刷新</button></div>
          <div class="identity-name">{{ config.alias }}</div><div class="muted">{{ info?.bluetoothMac ?? '等待读取设备标识' }}</div>
          <dl><div><dt>固件</dt><dd>{{ info?.firmwareVersion ?? '—' }}</dd></div><div><dt>产品 / 硬件</dt><dd>{{ info ? `${info.productId} / ${info.hardwareRevision}` : '—' }}</dd></div><div><dt>ATT MTU</dt><dd>{{ status?.mtu ?? '—' }}</dd></div><div><dt>E22</dt><dd>{{ status?.radioReady ? '可用' : '不可用' }}</dd></div></dl>
        </article>
        <article class="metric red"><span>安全状态</span><strong>未接入</strong><small>断路器与 safety_manager 尚未实现</small></article>
        <article class="metric"><span>电池</span><strong>{{ batteryText }}</strong><small>{{ status ? `${status.batteryMv} mV` : '等待设备状态' }}</small></article>
        <article class="metric"><span>运行时间</span><strong>{{ status ? `${status.uptimeSeconds}s` : '—' }}</strong><small>OTA：{{ status?.otaState ?? 0 }}</small></article>
      </section>

      <section class="two-column">
        <article class="card"><div class="card-head"><div><span class="section-number">01</span><h2>基础参数</h2></div></div>
          <label>设备别名<input v-model.trim="config.alias" maxlength="31" /></label>
          <label>状态上报周期（ms）<input v-model.number="config.statusPeriodMs" type="number" min="1000" max="60000" step="1000" /></label>
          <label>E22 发送超时（ms）<input v-model.number="config.radioTxTimeoutMs" type="number" min="100" max="5000" step="100" /></label>
          <button class="primary full" :disabled="!connected || busy" @click="saveConfig">验证并写入 NVS</button>
        </article>

        <article class="card"><div class="card-head"><div><span class="section-number">02</span><h2>E22 诊断收发</h2></div><select v-model="radioMode"><option value="text">文本</option><option value="hex">HEX</option></select></div>
          <textarea v-model="radioInput" rows="4" :placeholder="radioMode === 'hex' ? '例：01 A0 FF' : '输入诊断数据，不得作为安全控制报文'" />
          <button class="primary" :disabled="!connected || !status?.radioReady || busy" @click="sendRadio">发送诊断包</button>
          <div class="radio-log"><div v-if="radioLog.length === 0" class="empty">暂无收发记录</div><div v-for="(item, index) in radioLog" :key="index" class="log-row"><b :class="item.direction.toLowerCase()">{{ item.direction }}</b><time>{{ item.time }}</time><code>{{ item.value }}</code></div></div>
        </article>
      </section>

      <section class="card ota-card"><div class="card-head"><div><span class="section-number">03</span><h2>BLE 固件升级</h2></div><span class="tag">开发级 OTA</span></div>
        <div class="ota-grid"><div><label>云端固件 URL<div class="inline"><input v-model.trim="firmwareUrl" type="url" placeholder="https://example.com/firmware.bin" /><button class="ghost" :disabled="busy || !firmwareUrl" @click="loadFirmwareUrl">下载</button></div></label>
          <div class="divider"><span>或者</span></div><label class="file-picker">选择本地 .bin<input type="file" accept=".bin,application/octet-stream" @change="chooseFirmware" /></label></div>
          <div><label>固件内嵌版本<input v-model.trim="firmwareVersion" placeholder="必须与构建版本完全一致" maxlength="31" /></label><div class="firmware-meta"><strong>{{ firmwareName || '尚未选择固件' }}</strong><span>{{ firmware ? `${(firmware.length / 1024).toFixed(1)} KiB` : '—' }}</span></div></div></div>
        <div class="progress"><div :style="{ width: `${ota.percent}%` }" /></div><div class="progress-meta"><span>{{ otaStateText }}</span><span>{{ ota.sent }} / {{ ota.total }} bytes · {{ ota.percent }}%</span></div>
        <button class="danger" :disabled="!connected || !firmware || !firmwareVersion || busy" @click="startOta">校验并开始升级</button>
        <p class="footnote">升级期间请保持页面前台、电源稳定和设备在通信范围内。当前构建尚未启用固件签名，不能用于量产安全升级。</p>
      </section>
    </main>
  </div>
</template>
