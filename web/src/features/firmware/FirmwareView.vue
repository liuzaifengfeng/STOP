<script setup lang="ts">
import type { useDeviceTool } from '../../app/useDeviceTool'
import { usePreferences } from '../../app/usePreferences'
defineProps<{ tool: ReturnType<typeof useDeviceTool> }>()
const preferences = usePreferences()
</script>
<template>
  <section class="panel form-panel firmware-panel">
    <div class="panel-title"><div><span class="section-kicker">FIRMWARE MANAGER</span><h2>{{ preferences.t('bleUpgrade') }}</h2></div><span class="warning-badge">{{ preferences.t('devOta') }}</span></div>
    <div class="firmware-grid"><div><label>{{ preferences.t('cloudUrl') }}<div class="inline-control"><input v-model.trim="tool.firmwareUrl.value" type="url" placeholder="https://example.com/firmware.bin"><button class="button secondary" :disabled="tool.busy.value || !tool.firmwareUrl.value" @click="tool.loadFirmwareUrl">{{ preferences.t('download') }}</button></div></label><div class="separator"><span>{{ preferences.t('or') }}</span></div><label class="file-drop"><b>{{ preferences.t('chooseLocal') }}</b><span>{{ preferences.t('firmwareSupport') }}</span><input type="file" accept=".bin,application/octet-stream" @change="tool.chooseFirmware"></label></div><div><label>{{ preferences.t('embeddedVersion') }}<input v-model.trim="tool.firmwareVersion.value" maxlength="31" :placeholder="preferences.t('versionPlaceholder')"></label><div class="firmware-file"><span>{{ preferences.t('selectedFirmware') }}</span><strong>{{ tool.firmwareName.value || preferences.t('noFile') }}</strong><small>{{ tool.firmware.value ? `${(tool.firmware.value.length / 1024).toFixed(1)} KiB` : '—' }}</small></div></div></div>
    <div class="progress-track"><div :style="{ width: `${tool.ota.percent}%` }"></div></div><div class="progress-meta"><span>{{ tool.otaStateText.value }}</span><span>{{ tool.ota.sent }} / {{ tool.ota.total }} bytes · {{ tool.ota.percent }}%</span></div>
    <button class="button danger-button wide" :disabled="!tool.connected.value || !tool.firmware.value || !tool.firmwareVersion.value || tool.busy.value" @click="tool.startOta">{{ preferences.t('startUpgrade') }}</button>
    <p class="warning-note">{{ preferences.t('otaWarning') }}</p>
  </section>
</template>
