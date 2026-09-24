<script setup lang="ts">
import { computed } from 'vue'
import { useDeviceTool, type ToolPage } from './app/useDeviceTool'
import DashboardView from './features/dashboard/DashboardView.vue'
import ConfigurationView from './features/configuration/ConfigurationView.vue'
import RadioView from './features/radio/RadioView.vue'
import FirmwareView from './features/firmware/FirmwareView.vue'
import SettingsView from './features/settings/SettingsView.vue'
import { usePreferences } from './app/usePreferences'

const tool = useDeviceTool()
const preferences = usePreferences()
const pages = computed<Array<{ id: ToolPage; icon: string; label: string; caption: string }>>(() => [
  { id: 'dashboard', icon: '⌂', label: preferences.t('dashboard'), caption: preferences.t('dashboardCaption') },
  { id: 'configuration', icon: '⚙', label: preferences.t('configuration'), caption: preferences.t('configurationCaption') },
  { id: 'radio', icon: '⌁', label: preferences.t('radio'), caption: preferences.t('radioCaption') },
  { id: 'firmware', icon: '⇧', label: preferences.t('firmware'), caption: preferences.t('firmwareCaption') },
  { id: 'settings', icon: '●', label: preferences.t('settings'), caption: preferences.t('settingsCaption') },
])
const pageTitle = computed(() => pages.value.find((page) => page.id === tool.activePage.value)?.label ?? preferences.t('dashboard'))
const pageComponent = computed(() => ({ dashboard: DashboardView, configuration: ConfigurationView, radio: RadioView, firmware: FirmwareView, settings: SettingsView })[tool.activePage.value])
</script>

<template>
  <div :class="['app-shell', `theme-${preferences.resolvedTheme.value}`, `sidebar-${preferences.sidebar.value}`]">
    <aside class="sidebar">
      <div class="brand"><span class="brand-mark">ST</span><div><strong>STOP Toolbox</strong><small>DEVICE CONSOLE</small></div></div>
      <nav class="main-nav" aria-label="主要功能">
        <button v-for="page in pages" :key="page.id" :class="{ active: tool.activePage.value === page.id }" @click="tool.activePage.value = page.id">
          <span class="nav-icon">{{ page.icon }}</span><span class="nav-copy"><b>{{ page.label }}</b><small>{{ page.caption }}</small></span>
        </button>
      </nav>
      <div class="sidebar-foot">
        <div class="device-mini"><span :class="['status-light', { online: tool.connected.value }]" /><div><b>{{ tool.connected.value ? tool.deviceName.value : preferences.t('noDevice') }}</b><small>{{ tool.connected.value ? preferences.t('bleConnected') : preferences.t('waitingConnection') }}</small></div></div>
        <p>{{ preferences.t('maintenancePlane') }}<br>{{ preferences.t('safetyLock') }}</p>
      </div>
    </aside>
    <section class="workspace">
      <header class="workspace-header">
        <div><span class="eyebrow">STOP-C6 / MAINTENANCE</span><h1>{{ pageTitle }}</h1></div>
        <div class="connection-actions">
          <template v-if="tool.connected.value">
            <span :class="['signal-pill', `signal-${tool.signalStrength.value.level}`]">{{ tool.status.value?.rssi == null ? 'RSSI —' : `${tool.status.value.rssi} dBm` }}</span>
            <span :class="['signal-pill', `signal-${tool.linkQuality.value.level}`]" :title="tool.linkQuality.value.title">{{ preferences.t('quality') }} {{ tool.linkQuality.value.score ?? '—' }}</span>
            <button class="button secondary" @click="tool.disconnect">{{ preferences.t('disconnect') }}</button>
          </template>
          <button v-else class="button primary" :disabled="tool.busy.value" @click="tool.connect">{{ preferences.t('connect') }}</button>
        </div>
      </header>
      <div class="safety-strip"><b>{{ preferences.t('safetyBoundary') }}</b><span>{{ preferences.t('safetyCopy') }}</span></div>
      <div class="notice-bar"><span :class="['status-light', { online: tool.connected.value }]" />{{ tool.notice.value }}</div>
      <main class="page-content"><component :is="pageComponent" :tool="tool" /></main>
    </section>
  </div>
</template>
