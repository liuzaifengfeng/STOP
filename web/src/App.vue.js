import { computed } from 'vue';
import { useDeviceTool } from './app/useDeviceTool';
import DashboardView from './features/dashboard/DashboardView.vue';
import ConfigurationView from './features/configuration/ConfigurationView.vue';
import RadioView from './features/radio/RadioView.vue';
import FirmwareView from './features/firmware/FirmwareView.vue';
import SettingsView from './features/settings/SettingsView.vue';
import { usePreferences } from './app/usePreferences';
const tool = useDeviceTool();
const preferences = usePreferences();
const pages = computed(() => [
    { id: 'dashboard', icon: '⌂', label: preferences.t('dashboard'), caption: preferences.t('dashboardCaption') },
    { id: 'configuration', icon: '⚙', label: preferences.t('configuration'), caption: preferences.t('configurationCaption') },
    { id: 'radio', icon: '⌁', label: preferences.t('radio'), caption: preferences.t('radioCaption') },
    { id: 'firmware', icon: '⇧', label: preferences.t('firmware'), caption: preferences.t('firmwareCaption') },
    { id: 'settings', icon: '●', label: preferences.t('settings'), caption: preferences.t('settingsCaption') },
]);
const pageTitle = computed(() => pages.value.find((page) => page.id === tool.activePage.value)?.label ?? preferences.t('dashboard'));
const pageComponent = computed(() => ({ dashboard: DashboardView, configuration: ConfigurationView, radio: RadioView, firmware: FirmwareView, settings: SettingsView })[tool.activePage.value]);
const __VLS_ctx = {
    ...{},
    ...{},
};
let __VLS_components;
let __VLS_intrinsics;
let __VLS_directives;
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
    ...{ class: (['app-shell', `theme-${__VLS_ctx.preferences.resolvedTheme.value}`, `sidebar-${__VLS_ctx.preferences.sidebar.value}`]) },
});
/** @type {__VLS_StyleScopedClasses['app-shell']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.aside, __VLS_intrinsics.aside)({
    ...{ class: "sidebar" },
});
/** @type {__VLS_StyleScopedClasses['sidebar']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
    ...{ class: "brand" },
});
/** @type {__VLS_StyleScopedClasses['brand']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.span, __VLS_intrinsics.span)({
    ...{ class: "brand-mark" },
});
/** @type {__VLS_StyleScopedClasses['brand-mark']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.strong, __VLS_intrinsics.strong)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.small, __VLS_intrinsics.small)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.nav, __VLS_intrinsics.nav)({
    ...{ class: "main-nav" },
    'aria-label': "主要功能",
});
/** @type {__VLS_StyleScopedClasses['main-nav']} */ ;
for (const [page] of __VLS_vFor((__VLS_ctx.pages))) {
    __VLS_asFunctionalElement1(__VLS_intrinsics.button, __VLS_intrinsics.button)({
        ...{ onClick: (...[$event]) => {
                return (__VLS_ctx.tool.activePage.value = page.id);
                // @ts-ignore
                [preferences, preferences, pages, tool,];
            } },
        key: (page.id),
        ...{ class: ({ active: __VLS_ctx.tool.activePage.value === page.id }) },
    });
    /** @type {__VLS_StyleScopedClasses['active']} */ ;
    __VLS_asFunctionalElement1(__VLS_intrinsics.span, __VLS_intrinsics.span)({
        ...{ class: "nav-icon" },
    });
    /** @type {__VLS_StyleScopedClasses['nav-icon']} */ ;
    (page.icon);
    __VLS_asFunctionalElement1(__VLS_intrinsics.span, __VLS_intrinsics.span)({
        ...{ class: "nav-copy" },
    });
    /** @type {__VLS_StyleScopedClasses['nav-copy']} */ ;
    __VLS_asFunctionalElement1(__VLS_intrinsics.b, __VLS_intrinsics.b)({});
    (page.label);
    __VLS_asFunctionalElement1(__VLS_intrinsics.small, __VLS_intrinsics.small)({});
    (page.caption);
    // @ts-ignore
    [tool,];
}
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
    ...{ class: "sidebar-foot" },
});
/** @type {__VLS_StyleScopedClasses['sidebar-foot']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
    ...{ class: "device-mini" },
});
/** @type {__VLS_StyleScopedClasses['device-mini']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.span)({
    ...{ class: (['status-light', { online: __VLS_ctx.tool.connected.value }]) },
});
/** @type {__VLS_StyleScopedClasses['online']} */ ;
/** @type {__VLS_StyleScopedClasses['status-light']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.b, __VLS_intrinsics.b)({});
(__VLS_ctx.tool.connected.value ? __VLS_ctx.tool.deviceName.value : __VLS_ctx.preferences.t('noDevice'));
__VLS_asFunctionalElement1(__VLS_intrinsics.small, __VLS_intrinsics.small)({});
(__VLS_ctx.tool.connected.value ? __VLS_ctx.preferences.t('bleConnected') : __VLS_ctx.preferences.t('waitingConnection'));
__VLS_asFunctionalElement1(__VLS_intrinsics.p, __VLS_intrinsics.p)({});
(__VLS_ctx.preferences.t('maintenancePlane'));
__VLS_asFunctionalElement1(__VLS_intrinsics.br)({});
(__VLS_ctx.preferences.t('safetyLock'));
__VLS_asFunctionalElement1(__VLS_intrinsics.section, __VLS_intrinsics.section)({
    ...{ class: "workspace" },
});
/** @type {__VLS_StyleScopedClasses['workspace']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.header, __VLS_intrinsics.header)({
    ...{ class: "workspace-header" },
});
/** @type {__VLS_StyleScopedClasses['workspace-header']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.span, __VLS_intrinsics.span)({
    ...{ class: "eyebrow" },
});
/** @type {__VLS_StyleScopedClasses['eyebrow']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.h1, __VLS_intrinsics.h1)({});
(__VLS_ctx.pageTitle);
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
    ...{ class: "connection-actions" },
});
/** @type {__VLS_StyleScopedClasses['connection-actions']} */ ;
if (__VLS_ctx.tool.connected.value) {
    __VLS_asFunctionalElement1(__VLS_intrinsics.span, __VLS_intrinsics.span)({
        ...{ class: (['signal-pill', `signal-${__VLS_ctx.tool.signalStrength.value.level}`]) },
    });
    /** @type {__VLS_StyleScopedClasses['signal-pill']} */ ;
    (__VLS_ctx.tool.status.value?.rssi == null ? 'RSSI —' : `${__VLS_ctx.tool.status.value.rssi} dBm`);
    __VLS_asFunctionalElement1(__VLS_intrinsics.span, __VLS_intrinsics.span)({
        ...{ class: (['signal-pill', `signal-${__VLS_ctx.tool.linkQuality.value.level}`]) },
        title: (__VLS_ctx.tool.linkQuality.value.title),
    });
    /** @type {__VLS_StyleScopedClasses['signal-pill']} */ ;
    (__VLS_ctx.preferences.t('quality'));
    (__VLS_ctx.tool.linkQuality.value.score ?? '—');
    __VLS_asFunctionalElement1(__VLS_intrinsics.button, __VLS_intrinsics.button)({
        ...{ onClick: (__VLS_ctx.tool.disconnect) },
        ...{ class: "button secondary" },
    });
    /** @type {__VLS_StyleScopedClasses['button']} */ ;
    /** @type {__VLS_StyleScopedClasses['secondary']} */ ;
    (__VLS_ctx.preferences.t('disconnect'));
}
else {
    __VLS_asFunctionalElement1(__VLS_intrinsics.button, __VLS_intrinsics.button)({
        ...{ onClick: (__VLS_ctx.tool.connect) },
        ...{ class: "button primary" },
        disabled: (__VLS_ctx.tool.busy.value),
    });
    /** @type {__VLS_StyleScopedClasses['button']} */ ;
    /** @type {__VLS_StyleScopedClasses['primary']} */ ;
    (__VLS_ctx.preferences.t('connect'));
}
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
    ...{ class: "safety-strip" },
});
/** @type {__VLS_StyleScopedClasses['safety-strip']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.b, __VLS_intrinsics.b)({});
(__VLS_ctx.preferences.t('safetyBoundary'));
__VLS_asFunctionalElement1(__VLS_intrinsics.span, __VLS_intrinsics.span)({});
(__VLS_ctx.preferences.t('safetyCopy'));
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
    ...{ class: "notice-bar" },
});
/** @type {__VLS_StyleScopedClasses['notice-bar']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.span)({
    ...{ class: (['status-light', { online: __VLS_ctx.tool.connected.value }]) },
});
/** @type {__VLS_StyleScopedClasses['online']} */ ;
/** @type {__VLS_StyleScopedClasses['status-light']} */ ;
(__VLS_ctx.tool.notice.value);
__VLS_asFunctionalElement1(__VLS_intrinsics.main, __VLS_intrinsics.main)({
    ...{ class: "page-content" },
});
/** @type {__VLS_StyleScopedClasses['page-content']} */ ;
const __VLS_0 = (__VLS_ctx.pageComponent);
// @ts-ignore
const __VLS_1 = __VLS_asFunctionalComponent1(__VLS_0, new __VLS_0({
    tool: (__VLS_ctx.tool),
}));
const __VLS_2 = __VLS_1({
    tool: (__VLS_ctx.tool),
}, ...__VLS_functionalComponentArgsRest(__VLS_1));
// @ts-ignore
[preferences, preferences, preferences, preferences, preferences, preferences, preferences, preferences, preferences, preferences, tool, tool, tool, tool, tool, tool, tool, tool, tool, tool, tool, tool, tool, tool, tool, tool, tool, pageTitle, pageComponent,];
const __VLS_export = (await import('vue')).defineComponent({});
export default {};
