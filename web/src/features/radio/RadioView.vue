<script setup lang="ts">
import type { useDeviceTool } from '../../app/useDeviceTool'
import { usePreferences } from '../../app/usePreferences'
defineProps<{ tool: ReturnType<typeof useDeviceTool> }>()
const preferences = usePreferences()
</script>
<template>
  <div class="page-stack">
    <section class="panel form-panel"><div class="panel-title"><div><span class="section-kicker">RADIO TERMINAL</span><h2>{{ preferences.t('radioSend') }}</h2></div><select v-model="tool.radioMode.value"><option value="text">{{ preferences.t('text') }}</option><option value="hex">HEX</option></select></div><p class="section-description">{{ preferences.t('diagnosticWarning') }}</p><textarea v-model="tool.radioInput.value" rows="5" :placeholder="tool.radioMode.value === 'hex' ? '01 A0 FF' : preferences.t('inputDiagnostic')"></textarea><button class="button primary" :disabled="!tool.connected.value || !tool.status.value?.radioReady || tool.busy.value" @click="tool.sendRadio">{{ preferences.t('sendPacket') }}</button></section>
    <section class="panel terminal-panel"><div class="panel-title"><div><span class="section-kicker">LIVE TRAFFIC</span><h2>{{ preferences.t('traffic') }}</h2></div><span class="state-badge">{{ tool.radioLog.value.length }} {{ preferences.t('frame') }}</span></div><div v-if="tool.radioLog.value.length === 0" class="empty-state"><strong>{{ preferences.t('noData') }}</strong><span>{{ preferences.t('noDataHint') }}</span></div><div v-else class="radio-log"><div v-for="(item, index) in tool.radioLog.value" :key="index" class="log-row"><b :class="item.direction.toLowerCase()">{{ item.direction }}</b><time>{{ item.time }}</time><code>{{ item.value }}</code></div></div></section>
  </div>
</template>
