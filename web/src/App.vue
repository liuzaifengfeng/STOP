<script setup lang="ts">
import { computed, ref } from 'vue'
import { useDeviceTool, type DeviceRole, type ToolPage } from './app/useDeviceTool'
import DashboardView from './features/dashboard/DashboardView.vue'
import PairingView from './features/pairing/PairingView.vue'
import ConfigurationView from './features/configuration/ConfigurationView.vue'
import RadioView from './features/radio/RadioView.vue'
import FirmwareView from './features/firmware/FirmwareView.vue'
import SettingsView from './features/settings/SettingsView.vue'
import InstructionsView from './features/instructions/InstructionsView.vue'
import { usePreferences } from './app/usePreferences'

const controllerTool = useDeviceTool('controller')
const receiverTool = useDeviceTool('receiver')
const selectedRole = ref<DeviceRole>('controller')
const activePage = ref<ToolPage>('dashboard')
const tool = computed(() => selectedRole.value === 'controller' ? controllerTool : receiverTool)
const preferences = usePreferences()
const pages = computed<Array<{ id: ToolPage; icon: string; label: string; caption: string }>>(() => [
  { id: 'dashboard', icon: '⌂', label: preferences.t('dashboard'), caption: preferences.t('dashboardCaption') },
  { id: 'pairing', icon: '⇄', label: preferences.t('pairing'), caption: preferences.t('pairingCaption') },
  { id: 'configuration', icon: '⚙', label: preferences.t('configuration'), caption: preferences.t('configurationCaption') },
  { id: 'radio', icon: '⌁', label: preferences.t('radio'), caption: preferences.t('radioCaption') },
  { id: 'firmware', icon: '⇧', label: preferences.t('firmware'), caption: preferences.t('firmwareCaption') },
  { id: 'instructions', icon: '?', label: preferences.t('instructions'), caption: preferences.t('instructionsCaption') },
  { id: 'settings', icon: '●', label: preferences.t('settings'), caption: preferences.t('settingsCaption') },
])
const pageTitle = computed(() => pages.value.find((page) => page.id === activePage.value)?.label ?? preferences.t('dashboard'))
const pageComponent = computed(() => ({ dashboard: DashboardView, pairing: PairingView, configuration: ConfigurationView, radio: RadioView, firmware: FirmwareView, instructions: InstructionsView, settings: SettingsView })[activePage.value])
</script>

<template>
  <div :class="['app-shell', `theme-${preferences.resolvedTheme.value}`, `sidebar-${preferences.sidebar.value}`]">
    <aside class="sidebar">
      <div class="brand"><span class="brand-mark">ST</span><div><strong>STOP Toolbox</strong><small>DEVICE CONSOLE</small></div></div>
      <nav class="main-nav" aria-label="主要功能">
        <button v-for="page in pages" :key="page.id" :title="page.label" :aria-label="page.label" :class="{ active: activePage === page.id }" @click="activePage = page.id">
          <span class="nav-icon">{{ page.icon }}</span><span class="nav-copy"><b>{{ page.label }}</b><small>{{ page.caption }}</small></span>
        </button>
      </nav>
      <div class="sidebar-foot">
        <div class="device-mini"><span :class="['status-light', { online: controllerTool.connected.value }]" /><div><b>控制端</b><small>{{ controllerTool.connected.value ? controllerTool.deviceName.value : preferences.t('waitingConnection') }}</small></div></div>
        <div class="device-mini"><span :class="['status-light', { online: receiverTool.connected.value }]" /><div><b>被控端</b><small>{{ receiverTool.connected.value ? receiverTool.deviceName.value : preferences.t('waitingConnection') }}</small></div></div>
        <p>{{ preferences.t('maintenancePlane') }}<br>{{ preferences.t('safetyLock') }}</p>
      </div>
    </aside>
    <section class="workspace">
      <header class="workspace-header">
        <div><span class="eyebrow">STOP-C6 / {{ selectedRole === 'controller' ? 'CONTROL' : 'RECEIVER' }}</span><h1>{{ pageTitle }}</h1></div>
        <div class="connection-actions">
          <template v-if="tool.connected.value">
            <span :class="['signal-pill', `signal-${tool.signalStrength.value.level}`]">{{ tool.status.value?.rssi == null ? 'RSSI —' : `${tool.status.value.rssi} dBm` }}</span>
            <span :class="['signal-pill', `signal-${tool.linkQuality.value.level}`]" :title="tool.linkQuality.value.title">{{ preferences.t('quality') }} {{ tool.linkQuality.value.score ?? '—' }}</span>
          </template>
        </div>
      </header>
      <div class="device-switcher">
        <div v-for="entry in [{ role: 'controller' as const, label: '控制端', session: controllerTool }, { role: 'receiver' as const, label: '被控端', session: receiverTool }]"
             :key="entry.role" :class="['device-slot', { selected: selectedRole === entry.role }]" @click="selectedRole = entry.role">
          <span :class="['status-light', { online: entry.session.connected.value }]" />
          <div><b>{{ entry.label }}</b><small>{{ entry.session.info.value?.bluetoothMac ?? entry.session.deviceName.value }}</small><small>{{ entry.session.notice.value }}</small></div>
          <button v-if="entry.session.connected.value" class="button secondary" @click.stop="entry.session.disconnect">{{ preferences.t('disconnect') }}</button>
          <button v-else class="button primary" :disabled="entry.session.busy.value" @click.stop="entry.session.connect">{{ preferences.t('connect') }}</button>
        </div>
      </div>
      <div class="safety-strip"><b>{{ preferences.t('safetyBoundary') }}</b><span>{{ preferences.t('safetyCopy') }}</span></div>
      <div class="notice-bar"><span :class="['status-light', { online: tool.connected.value }]" />{{ tool.notice.value }}</div>
      <main class="page-content"><component :is="pageComponent" :tool="tool" :controller="controllerTool" :receiver="receiverTool" /></main>
    </section>
  </div>
</template>
