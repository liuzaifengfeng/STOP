<script setup lang="ts">
import { ref } from 'vue'
import type { useDeviceTool } from '../../app/useDeviceTool'
import { usePreferences } from '../../app/usePreferences'
const props = defineProps<{
  tool: ReturnType<typeof useDeviceTool>
  controller: ReturnType<typeof useDeviceTool>
  receiver: ReturnType<typeof useDeviceTool>
}>()
const preferences = usePreferences()
const preparing = ref(false)
const radioHint = ref('')

async function sendPacket() {
  if (preparing.value) return
  if (!props.tool.radioInput.value.trim()) { radioHint.value = '请先输入诊断数据'; return }
  preparing.value = true
  try {
    if (props.tool.role === 'receiver' && props.controller.connected.value) {
      if (!await props.receiver.refresh()) { radioHint.value = '无法读取被控端输出状态，已取消发送'; return }
      if (props.receiver.status.value?.safetyState === 3) { radioHint.value = '被控端输出已接通，不能暂停安全心跳'; return }
      radioHint.value = '控制端进入 10 秒接收窗口，暂停安全心跳后发送…'
      if (!await props.controller.startRadioListen()) {
        radioHint.value = `控制端未进入接收窗口：${props.controller.notice.value}`
        return
      }
      await new Promise((resolve) => window.setTimeout(resolve, 500))
    }
    const sent = await props.tool.sendRadio()
    radioHint.value = sent
      ? props.tool.role === 'receiver' && props.controller.connected.value
        ? '已发送；控制端接收窗口将在 10 秒后自动关闭'
        : ''
      : props.tool.notice.value
  } finally { preparing.value = false }
}
</script>
<template>
  <div class="page-stack">
    <section class="panel form-panel"><div class="panel-title"><div><span class="section-kicker">RADIO TERMINAL</span><h2>{{ preferences.t('radioSend') }}</h2></div><select v-model="tool.radioMode.value"><option value="text">{{ preferences.t('text') }}</option><option value="hex">HEX</option></select></div><p class="section-description">{{ preferences.t('diagnosticWarning') }} 被控端向已连接的控制端发送时，会先暂停控制端心跳 10 秒；被控端输出须断开。</p><textarea v-model="tool.radioInput.value" rows="5" :placeholder="tool.radioMode.value === 'hex' ? '01 A0 FF' : preferences.t('inputDiagnostic')"></textarea><button class="button primary" :disabled="preparing || !tool.connected.value || !tool.status.value?.radioReady || tool.busy.value" @click="sendPacket">{{ preferences.t('sendPacket') }}</button><p v-if="radioHint" class="section-description">{{ radioHint }}</p></section>
    <section class="panel terminal-panel"><div class="panel-title"><div><span class="section-kicker">LIVE TRAFFIC</span><h2>{{ preferences.t('traffic') }}</h2></div><span class="state-badge">{{ tool.radioLog.value.length }} {{ preferences.t('frame') }}</span></div><div v-if="tool.radioLog.value.length === 0" class="empty-state"><strong>{{ preferences.t('noData') }}</strong><span>{{ preferences.t('noDataHint') }}</span></div><div v-else class="radio-log"><div v-for="(item, index) in tool.radioLog.value" :key="index" class="log-row"><b :class="item.direction.toLowerCase()">{{ item.direction }}</b><time>{{ item.time }}</time><code>{{ item.value }}</code></div></div></section>
  </div>
</template>
