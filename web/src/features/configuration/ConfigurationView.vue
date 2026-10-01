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
    <template v-if="tool.config.radioLink !== undefined">
      <label>无线工作模式（重启生效）
        <select v-model.number="tool.config.radioLink">
          <option :value="0">E22 单通道</option>
          <option :value="1">ESP-NOW 单通道</option>
          <option :value="2">E22 + ESP-NOW 双通道</option>
        </select>
      </label>
      <label>ESP-NOW 信道（两端一致，重启生效）<input v-model.number="tool.config.espnowChannel" type="number" min="1" max="11" step="1"></label>
      <p class="section-description">当前运行：{{ tool.status.value?.radioMode === 6 ? 'E22 + ESP-NOW 双通道' : tool.status.value?.radioMode === 5 ? 'ESP-NOW' : 'E22' }}。下方保存的是下次启动配置，运行中的链路不会切换。请先断开被控端输出，分别配置两端并重新上电，再本地确认使能。双通道任一路有效心跳均可维持连接，不按估计距离切换。</p>
    </template>
    <p v-else class="section-description">连接支持无线模式配置的新固件后，可选择 E22、ESP-NOW 或双通道。</p>
    <button class="button primary wide" :disabled="!tool.connected.value || tool.busy.value" @click="tool.saveConfig">{{ preferences.t('saveDevice') }}</button>
  </section>
</template>
