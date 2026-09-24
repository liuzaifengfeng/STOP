<script setup lang="ts">
import { usePreferences, type Language, type SidebarMode, type ThemeMode } from '../../app/usePreferences'

const preferences = usePreferences()
const sidebarOptions: SidebarMode[] = ['expanded', 'auto', 'collapsed']
const themeOptions: ThemeMode[] = ['system', 'light', 'dark']
const sidebarLabels = { expanded: 'expanded', auto: 'automatic', collapsed: 'collapsed' } as const
const themeLabels = { system: 'system', light: 'light', dark: 'dark' } as const
</script>

<template>
  <section class="settings-page">
    <div class="settings-group">
      <h2>{{ preferences.t('sidebarBehavior') }}</h2>
      <div class="segmented-control">
        <button v-for="option in sidebarOptions" :key="option" :class="{ selected: preferences.sidebar.value === option }" @click="preferences.sidebar.value = option">{{ preferences.t(sidebarLabels[option]) }}</button>
      </div>
    </div>
    <div class="settings-group">
      <h2>{{ preferences.t('theme') }}</h2>
      <div class="segmented-control">
        <button v-for="option in themeOptions" :key="option" :class="{ selected: preferences.theme.value === option }" @click="preferences.theme.value = option">{{ preferences.t(themeLabels[option]) }}</button>
      </div>
    </div>
    <div class="settings-group">
      <h2>{{ preferences.t('accent') }}</h2>
      <div class="setting-select"><span class="accent-dot" />{{ preferences.t('accentDefault') }}<span>⌄</span></div>
    </div>
    <div class="settings-group">
      <h2>{{ preferences.t('switchLanguage') }}</h2>
      <select class="language-select" :value="preferences.language.value" @change="preferences.language.value = ($event.target as HTMLSelectElement).value as Language">
        <option value="zh-CN">中文</option><option value="en">English</option>
      </select>
    </div>
    <p class="settings-footnote">{{ preferences.t('savedAutomatically') }}</p>
  </section>
</template>
