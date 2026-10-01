<script setup lang="ts">
import type { useDeviceTool } from '../../app/useDeviceTool'
import { usePreferences } from '../../app/usePreferences'
defineProps<{ tool: ReturnType<typeof useDeviceTool> }>()
const preferences = usePreferences()
const receiverStates: Record<number, string> = {
  1: '未配对', 2: '断电待确认', 3: '输出已接通',
  4: '无线失联', 5: '硬件故障', 6: '配对中',
}
const receiverState = (state?: number) => receiverStates[state ?? 0] ?? '状态未知'
</script>
<template>
  <div class="page-stack">
    <section class="device-hero panel">
      <div class="device-identity"><span class="section-kicker">{{ preferences.t('connectedDevice') }}</span><h2>{{ tool.config.alias }}</h2><code>{{ tool.info.value?.bluetoothMac ?? preferences.t('waitingIdentity') }}</code><div class="chip-row"><span>ESP32-C6</span><span>BLE</span><span>{{ tool.status.value?.radioMode === 6 ? 'E22 + ESP-NOW' : tool.status.value?.radioMode === 5 ? 'ESP-NOW' : 'E22' }} {{ tool.status.value?.radioReady ? 'READY' : 'OFFLINE' }}</span></div></div>
      <div class="device-visual" aria-hidden="true"><div class="antenna" /><div class="device-body"><i /><span>STOP</span><small>C6</small></div></div>
      <button class="button secondary refresh-button" :disabled="!tool.connected.value || tool.busy.value" @click="tool.refresh">{{ preferences.t('refresh') }}</button>
    </section>
    <section class="metric-grid">
      <article class="metric-card danger"><span>{{ preferences.t('safetyStatus') }}</span><strong>{{ tool.info.value?.productId === 2 ? receiverState(tool.status.value?.safetyState) : tool.status.value?.pairFlags == null ? '状态未知' : (tool.status.value.pairFlags & 2) ? '急停已释放' : '急停按下 / 断开' }}</strong><small>{{ tool.info.value?.productId === 2 ? '上电后收到控制端释放状态和连续安全心跳，自动供电' : '以 GPIO6 常闭触点实测状态为准' }}</small></article>
      <article v-if="tool.info.value?.productId === 2" class="metric-card"><span>输入电压 VIN</span><strong>{{ tool.status.value?.receiverVinDisabled ? '已禁用' : tool.status.value?.receiverVinMv == null ? '—' : `${(tool.status.value.receiverVinMv / 1000).toFixed(2)} V` }}</strong><small>{{ tool.status.value?.receiverVinDisabled ? 'ADC 异常，输入电压保护临时禁用' : 'GPIO0 分压测量' }}</small></article>
      <article v-else class="metric-card"><span>{{ preferences.t('battery') }}</span><strong>{{ tool.batteryText.value }}</strong><small>{{ tool.status.value ? `${tool.status.value.batteryMv} mV` : preferences.t('waitingStatus') }}</small></article>
      <article v-if="tool.info.value?.productId === 2" class="metric-card"><span>输出电压 VOUT</span><strong>{{ tool.status.value ? `${(tool.status.value.batteryMv / 1000).toFixed(2)} V` : '—' }}</strong><small>INA226 母线测量</small></article>
      <article v-if="tool.info.value?.productId === 2" class="metric-card"><span>输出电流</span><strong>{{ tool.status.value?.receiverInaValid ? `${(tool.status.value.receiverCurrentMa! / 1000).toFixed(2)} A` : '—' }}</strong><small>{{ tool.status.value?.receiverOverCurrent ? '过流告警' : 'INA226 分流电阻测量' }}</small></article>
      <article class="metric-card"><span>{{ preferences.t('uptime') }}</span><strong>{{ tool.status.value ? `${tool.status.value.uptimeSeconds}s` : '—' }}</strong><small>{{ preferences.t('uptimeHint') }}</small></article>
      <article class="metric-card"><span>ATT MTU</span><strong>{{ tool.status.value?.mtu ?? '—' }}</strong><small>{{ preferences.t('currentValue') }}</small></article>
    </section>
    <section class="panel details-panel">
      <div class="panel-title"><div><span class="section-kicker">PAIR LINK</span><h2>无线配对</h2></div><span class="state-badge">{{ tool.status.value?.pairFlags == null ? '旧版状态' : (tool.status.value.pairFlags & 1) ? '已配对' : '未配对' }}</span></div>
      <dl class="detail-list"><div><dt>本机 MAC</dt><dd>{{ tool.info.value?.bluetoothMac ?? '—' }}</dd></div><div><dt>配对目标 MAC</dt><dd>{{ tool.status.value?.peerMac ?? '—' }}</dd></div><div><dt>安全心跳</dt><dd>{{ tool.info.value?.productId === 2 ? ((tool.status.value?.pairFlags ?? 0) & 4 ? '有效' : '未收到 / 超时') : '在配对与联调页查看' }}</dd></div></dl>
    </section>
    <section class="panel details-panel">
      <div class="panel-title"><div><span class="section-kicker">DEVICE INFORMATION</span><h2>{{ preferences.t('deviceInfo') }}</h2></div><span class="state-badge">{{ tool.connected.value ? 'ONLINE' : 'OFFLINE' }}</span></div>
      <dl class="detail-list"><div><dt>{{ preferences.t('firmwareVersion') }}</dt><dd>{{ tool.info.value?.firmwareVersion ?? '—' }}</dd></div><div><dt>{{ preferences.t('productId') }}</dt><dd>{{ tool.info.value?.productId ?? '—' }}</dd></div><div><dt>{{ preferences.t('hardwareRevision') }}</dt><dd>{{ tool.info.value?.hardwareRevision ?? '—' }}</dd></div><div><dt>{{ preferences.t('protocolVersion') }}</dt><dd>{{ tool.info.value?.protocolVersion ?? '—' }}</dd></div><div><dt>{{ preferences.t('radioModule') }}</dt><dd>{{ tool.status.value?.radioReady ? preferences.t('available') : preferences.t('notAvailable') }}</dd></div><div><dt>{{ preferences.t('otaStatus') }}</dt><dd>{{ tool.status.value?.otaState ?? 0 }}</dd></div></dl>
    </section>
  </div>
</template>
