<script setup lang="ts">
import type { useDeviceTool } from '../../app/useDeviceTool'
import { usePreferences } from '../../app/usePreferences'
defineProps<{ tool: ReturnType<typeof useDeviceTool> }>()
const preferences = usePreferences()
</script>
<template>
  <section class="panel form-panel narrow-panel">
    <div class="panel-title"><div><span class="section-kicker">DEVICE SETTINGS</span><h2>{{ preferences.t('basicParameters') }}</h2></div><span class="state-badge">NVS</span></div>
    <p class="section-description">{{ preferences.t('storedNvs') }}</p>
    <label>{{ preferences.t('alias') }}<input v-model.trim="tool.config.alias" maxlength="31"></label>
    <label>{{ preferences.t('statusPeriod') }}<input v-model.number="tool.config.statusPeriodMs" type="number" min="1000" max="60000" step="1000"></label>
    <label>{{ preferences.t('radioTimeout') }}<input v-model.number="tool.config.radioTxTimeoutMs" type="number" min="100" max="5000" step="100"></label>
    <button class="button primary wide" :disabled="!tool.connected.value || tool.busy.value" @click="tool.saveConfig">{{ preferences.t('saveDevice') }}</button>
  </section>
</template>
