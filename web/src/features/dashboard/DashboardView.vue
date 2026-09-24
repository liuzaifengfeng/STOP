<script setup lang="ts">
import type { useDeviceTool } from '../../app/useDeviceTool'
import { usePreferences } from '../../app/usePreferences'
defineProps<{ tool: ReturnType<typeof useDeviceTool> }>()
const preferences = usePreferences()
</script>
<template>
  <div class="page-stack">
    <section class="device-hero panel">
      <div class="device-identity"><span class="section-kicker">{{ preferences.t('connectedDevice') }}</span><h2>{{ tool.config.alias }}</h2><code>{{ tool.info.value?.bluetoothMac ?? preferences.t('waitingIdentity') }}</code><div class="chip-row"><span>ESP32-C6</span><span>BLE</span><span>{{ tool.status.value?.radioReady ? 'E22 READY' : 'E22 OFFLINE' }}</span></div></div>
      <div class="device-visual" aria-hidden="true"><div class="antenna" /><div class="device-body"><i /><span>STOP</span><small>C6</small></div></div>
      <button class="button secondary refresh-button" :disabled="!tool.connected.value || tool.busy.value" @click="tool.refresh">{{ preferences.t('refresh') }}</button>
    </section>
    <section class="metric-grid">
      <article class="metric-card danger"><span>{{ preferences.t('safetyStatus') }}</span><strong>{{ preferences.t('unavailable') }}</strong><small>{{ preferences.t('safetyMissing') }}</small></article>
      <article class="metric-card"><span>{{ preferences.t('battery') }}</span><strong>{{ tool.batteryText.value }}</strong><small>{{ tool.status.value ? `${tool.status.value.batteryMv} mV` : preferences.t('waitingStatus') }}</small></article>
      <article class="metric-card"><span>{{ preferences.t('uptime') }}</span><strong>{{ tool.status.value ? `${tool.status.value.uptimeSeconds}s` : '—' }}</strong><small>{{ preferences.t('uptimeHint') }}</small></article>
      <article class="metric-card"><span>ATT MTU</span><strong>{{ tool.status.value?.mtu ?? '—' }}</strong><small>{{ preferences.t('currentValue') }}</small></article>
    </section>
    <section class="panel details-panel">
      <div class="panel-title"><div><span class="section-kicker">DEVICE INFORMATION</span><h2>{{ preferences.t('deviceInfo') }}</h2></div><span class="state-badge">{{ tool.connected.value ? 'ONLINE' : 'OFFLINE' }}</span></div>
      <dl class="detail-list"><div><dt>{{ preferences.t('firmwareVersion') }}</dt><dd>{{ tool.info.value?.firmwareVersion ?? '—' }}</dd></div><div><dt>{{ preferences.t('productId') }}</dt><dd>{{ tool.info.value?.productId ?? '—' }}</dd></div><div><dt>{{ preferences.t('hardwareRevision') }}</dt><dd>{{ tool.info.value?.hardwareRevision ?? '—' }}</dd></div><div><dt>{{ preferences.t('protocolVersion') }}</dt><dd>{{ tool.info.value?.protocolVersion ?? '—' }}</dd></div><div><dt>{{ preferences.t('radioModule') }}</dt><dd>{{ tool.status.value?.radioReady ? preferences.t('available') : preferences.t('notAvailable') }}</dd></div><div><dt>{{ preferences.t('otaStatus') }}</dt><dd>{{ tool.status.value?.otaState ?? 0 }}</dd></div></dl>
    </section>
  </div>
</template>
