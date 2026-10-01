<script setup lang="ts">
import { computed, ref } from 'vue'
import type { useDeviceTool } from '../../app/useDeviceTool'
import { textBytes } from '../../protocol'

const props = defineProps<{
  controller: ReturnType<typeof useDeviceTool>
  receiver: ReturnType<typeof useDeviceTool>
}>()

const testResult = ref('尚未进行无线测试')
const testing = ref(false)
const pairResult = ref('')
const pairDataReady = computed(() => props.controller.status.value?.pairFlags != null &&
  props.receiver.status.value?.pairFlags != null)
const controllerPaired = computed(() => Boolean((props.controller.status.value?.pairFlags ?? 0) & 1))
const receiverPaired = computed(() => Boolean((props.receiver.status.value?.pairFlags ?? 0) & 1))
const matched = computed(() => controllerPaired.value && receiverPaired.value &&
  props.controller.status.value?.peerMac === props.receiver.info.value?.bluetoothMac &&
  props.receiver.status.value?.peerMac === props.controller.info.value?.bluetoothMac)
const receiverOutputOn = computed(() => props.receiver.status.value?.safetyState === 3)
const heartbeatFresh = computed(() => Boolean((props.receiver.status.value?.pairFlags ?? 0) & 4))
// 网页状态仅提示条件；使能最终由固件实时检查，避免旧状态让调试入口无法点击。
const debugHint = computed(() => {
  if (!props.receiver.connected.value) return '请先连接被控端'
  if (props.receiver.busy.value) return '被控端正在处理请求，请稍候'
  if (testing.value) return '无线联调进行中，请等待测试完成'
  if (receiverOutputOn.value) return '输出已经接通，可使用断开输出'
  const status = props.receiver.status.value
  if (status?.pairFlags == null) return '尚未读到完整状态，可刷新或提交使能请求'
  const reasons: string[] = []
  if (!receiverPaired.value) reasons.push('未配对')
  if (!heartbeatFresh.value) reasons.push('安全心跳未收到或已超时')
  if (!(status.pairFlags & 2)) reasons.push('控制端未允许输出（检查急停）')
  if (status.safetyState === 6) reasons.push('30 秒配对窗口尚未结束')
  if (status.safetyState === 5) reasons.push('硬件故障锁存')
  return reasons.length ? '当前可能阻止使能：' + reasons.join('；') + '。可点击提交，由固件确认。' : '网页条件正常；点击后由固件检查实时心跳和电源条件。'
})
const controllerSafe = computed(() => Boolean((props.controller.status.value?.pairFlags ?? 0) & 2))

async function refreshBoth() {
  await Promise.all([props.controller.refresh(), props.receiver.refresh()])
}

async function startPairing() {
  pairResult.value = '正在请求被控端进入配对模式…'
  const ok = await props.receiver.startPairing()
  pairResult.value = ok ? '被控端已开启 30 秒配对窗口，等待控制端自动发起配对。' : `开启失败：${props.receiver.notice.value}`
  if (ok && props.controller.connected.value) await props.controller.refresh()
}

async function testDirection(from: ReturnType<typeof useDeviceTool>, to: ReturnType<typeof useDeviceTool>, label: string, reverse = false) {
  if (testing.value || from.busy.value || to.busy.value) return
  testing.value = true
  try {
    if (!await props.receiver.refresh()) { testResult.value = '无法读取被控端输出状态，已取消测试'; return }
    if (receiverOutputOn.value) { testResult.value = '被控端输出已接通，已取消测试'; return }
    if (reverse) {
      testResult.value = '正在让控制端进入 10 秒接收窗口；被控端将保持断电…'
      if (!await props.controller.startRadioListen()) {
        testResult.value = `无法开启控制端接收窗口：${props.controller.notice.value}`
        return
      }
      await new Promise((resolve) => window.setTimeout(resolve, 500))
    }
    const marker = `STOP_TEST_${Date.now()}`
    const data = textBytes(marker)
    const received = () => to.radioLog.value.some((entry) => entry.direction === 'RX' && entry.value.includes(marker))
    // E22 半双工；诊断包以间隔重试，避免一次碰撞就误判链路故障。
    for (let attempt = 1; attempt <= 3; attempt += 1) {
      testResult.value = `${label}：第 ${attempt}/3 次发送中…`
      if (!await from.sendDiagnostic(data)) {
        testResult.value = `${label}：第 ${attempt} 次发送失败，查看发送端设备提示`
        return
      }
      for (let i = 0; i < 13; i += 1) {
        if (received()) {
          testResult.value = `${label}：接收成功（第 ${attempt} 次，${marker}）`
          return
        }
        await new Promise((resolve) => window.setTimeout(resolve, 100))
      }
      if (attempt < 3) await new Promise((resolve) => window.setTimeout(resolve, 250 + attempt * 130))
    }
    testResult.value = `${label}：3 次发送均未在接收端网页日志中看到；检查两端串口的“Diagnostic TX”和“Diagnostic RX”日志`
  } finally { testing.value = false }
}
</script>

<template>
  <div class="page-stack">
    <section class="panel">
      <div class="panel-title"><div><span class="section-kicker">PAIRING</span><h2>双端配对状态</h2></div><button class="button secondary" :disabled="!controller.connected.value || !receiver.connected.value" @click="refreshBoth">刷新两端</button></div>
      <p class="section-description">控制端会自动发送配对请求；可通过被控端本地按键或下方网页按钮打开配对窗口。配对不会远程接通输出。</p>
      <div class="pair-grid">
        <div class="pair-device"><b>控制端</b><span>{{ controller.connected.value ? 'BLE 已连接' : '未连接' }}</span><code>{{ controller.info.value?.bluetoothMac ?? '—' }}</code><small>配对：{{ pairDataReady ? (controllerPaired ? '已保存' : '未配对') : '待读取' }}</small><small>目标：{{ controller.status.value?.peerMac ?? '—' }}</small><small>急停触点：{{ pairDataReady ? (controllerSafe ? '正常' : '断开 / 按下') : '未知' }}</small></div>
        <div class="pair-device"><b>被控端</b><span>{{ receiver.connected.value ? 'BLE 已连接' : '未连接' }}</span><code>{{ receiver.info.value?.bluetoothMac ?? '—' }}</code><small>配对：{{ pairDataReady ? (receiverPaired ? '已保存' : '未配对') : '待读取' }}</small><small>目标：{{ receiver.status.value?.peerMac ?? '—' }}</small><small>心跳：{{ heartbeatFresh ? '有效' : '未收到 / 超时' }}</small><small>输出：{{ receiverOutputOn ? '已接通' : '断开' }}</small></div>
      </div>
      <p class="pair-verdict" :class="{ success: matched }">{{ !controller.connected.value || !receiver.connected.value ? '请分别连接两台设备' : !pairDataReady ? '设备固件尚未提供配对详情，请更新两端固件' : matched ? '已确认两端互相配对，MAC 地址一致' : receiverPaired && !controllerPaired ? '被控端已保存身份，但控制端尚未收到配对确认；请重新打开被控端配对窗口' : '尚未确认两端互配，请按下方步骤操作' }}</p>
    </section>
    <section class="panel">
      <div class="panel-title"><div><span class="section-kicker">WORKFLOW</span><h2>配对操作</h2></div></div>
      <div class="pair-actions"><button class="button primary" :disabled="!receiver.connected.value || receiver.busy.value || receiverOutputOn || !receiver.status.value?.radioReady" @click="startPairing">被控端进入配对模式（30 秒）</button></div>
      <p class="section-description">{{ pairResult || '也可用被控端本地按键进入配对模式。' }}</p>
      <ol class="pair-steps">
        <li>分别连接控制端和被控端，确认两端 E22 显示正常。</li>
        <li>保持被控端输出断开；若控制端已与其他设备配对，先按下急停。点击上方按钮，或按住被控端本地按键 8 秒；达到 8 秒时即打开 30 秒配对窗口。</li>
        <li>控制端自动发送配对请求。等待两端显示“已保存”，并检查上方目标 MAC 与对端 MAC 一致。</li>
        <li>联调后如需恢复供电，可在满足电源和心跳条件时，使用下方临时调试使能，或在被控端本地按住 3～7 秒后松开。</li>
      </ol>
      <p class="section-description">被控端当前状态：{{ receiver.status.value?.safetyState === 6 ? '配对窗口已打开' : '配对窗口未打开' }}。网页连接与 E22 配对是两条独立链路。</p>
    </section>
    <section class="panel">
      <div class="panel-title"><div><span class="section-kicker">TEMP DEBUG</span><h2>本地按钮替代 · 临时调试</h2></div></div>
      <p class="section-description">用于 GPIO6 按键故障时替代本地使能。点击使能会实际接通输出；固件仍检查配对、连续安全心跳、VOUT 小于 2V、电流和 OTA 状态；当前固件已临时禁用 VIN ADC 及输入欠压/过压检查。配对功能使用上方的 30 秒配对按钮。</p>
      <div class="pair-actions">
        <button class="button primary" :disabled="testing || !receiver.connected.value || receiver.busy.value || receiverOutputOn" :title="debugHint" @click="receiver.debugOutput(true)">临时调试：安全使能输出</button>
        <button class="button secondary" :disabled="!receiver.connected.value || receiver.busy.value" @click="receiver.debugOutput(false)">断开输出</button>
      </div>
      <p class="section-description">{{ debugHint }}</p>
      <p class="pair-verdict">{{ receiver.notice.value }} · 实际输出：{{ !receiver.connected.value || receiver.status.value?.pairFlags == null ? '未知，请连接并刷新' : receiverOutputOn ? '已接通' : '断开' }}</p>
      <p class="section-description">需更新被控端固件。使能被拒绝时查看串口安全日志；急停仍立即断开；上电、急停松开或链路恢复后，连续 3 个安全心跳且电源检查通过会自动供电。硬件故障、OTA 和网页手动断电仍阻止自动使能。</p>
    </section>
    <section class="panel">
      <div class="panel-title"><div><span class="section-kicker">RADIO TEST</span><h2>双向无线联调</h2></div></div>
      <p class="section-description">仅在被控端输出断开时发送普通诊断包。自动供电模式下，请先点击“断开输出”保持手动停机，再做无线测试。反向测试会让控制端暂停心跳 10 秒，随后恢复发送。</p>
      <div class="pair-actions"><button class="button primary" :disabled="testing || !controller.connected.value || !receiver.connected.value || receiverOutputOn || controller.busy.value || receiver.busy.value" @click="testDirection(controller, receiver, '控制端 → 被控端')">控制端 → 被控端</button><button class="button secondary" :disabled="testing || !controller.connected.value || !receiver.connected.value || receiverOutputOn || controller.busy.value || receiver.busy.value" @click="testDirection(receiver, controller, '被控端 → 控制端', true)">被控端 → 控制端</button></div>
      <p class="pair-verdict">{{ testResult }}</p>
      <p class="section-description">心跳 {{ heartbeatFresh ? '有效' : '未确认' }}；{{ matched ? 'MAC 配对一致' : 'MAC 配对未确认' }}；被控输出 {{ receiverOutputOn ? '接通' : '断开' }}。</p>
    </section>
  </div>
</template>
