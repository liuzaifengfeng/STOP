import { usePreferences } from '../../app/usePreferences';
const __VLS_props = defineProps();
const preferences = usePreferences();
const __VLS_ctx = {
    ...{},
    ...{},
    ...{},
    ...{},
};
let __VLS_components;
let __VLS_intrinsics;
let __VLS_directives;
__VLS_asFunctionalElement1(__VLS_intrinsics.section, __VLS_intrinsics.section)({
    ...{ class: "panel form-panel firmware-panel" },
});
/** @type {__VLS_StyleScopedClasses['panel']} */ ;
/** @type {__VLS_StyleScopedClasses['form-panel']} */ ;
/** @type {__VLS_StyleScopedClasses['firmware-panel']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
    ...{ class: "panel-title" },
});
/** @type {__VLS_StyleScopedClasses['panel-title']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.span, __VLS_intrinsics.span)({
    ...{ class: "section-kicker" },
});
/** @type {__VLS_StyleScopedClasses['section-kicker']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.h2, __VLS_intrinsics.h2)({});
(__VLS_ctx.preferences.t('bleUpgrade'));
__VLS_asFunctionalElement1(__VLS_intrinsics.span, __VLS_intrinsics.span)({
    ...{ class: "warning-badge" },
});
/** @type {__VLS_StyleScopedClasses['warning-badge']} */ ;
(__VLS_ctx.preferences.t('devOta'));
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
    ...{ class: "firmware-grid" },
});
/** @type {__VLS_StyleScopedClasses['firmware-grid']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.label, __VLS_intrinsics.label)({});
(__VLS_ctx.preferences.t('cloudUrl'));
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
    ...{ class: "inline-control" },
});
/** @type {__VLS_StyleScopedClasses['inline-control']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.input)({
    type: "url",
    placeholder: "https://example.com/firmware.bin",
});
(__VLS_ctx.tool.firmwareUrl.value);
__VLS_asFunctionalElement1(__VLS_intrinsics.button, __VLS_intrinsics.button)({
    ...{ onClick: (__VLS_ctx.tool.loadFirmwareUrl) },
    ...{ class: "button secondary" },
    disabled: (__VLS_ctx.tool.busy.value || !__VLS_ctx.tool.firmwareUrl.value),
});
/** @type {__VLS_StyleScopedClasses['button']} */ ;
/** @type {__VLS_StyleScopedClasses['secondary']} */ ;
(__VLS_ctx.preferences.t('download'));
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
    ...{ class: "separator" },
});
/** @type {__VLS_StyleScopedClasses['separator']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.span, __VLS_intrinsics.span)({});
(__VLS_ctx.preferences.t('or'));
__VLS_asFunctionalElement1(__VLS_intrinsics.label, __VLS_intrinsics.label)({
    ...{ class: "file-drop" },
});
/** @type {__VLS_StyleScopedClasses['file-drop']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.b, __VLS_intrinsics.b)({});
(__VLS_ctx.preferences.t('chooseLocal'));
__VLS_asFunctionalElement1(__VLS_intrinsics.span, __VLS_intrinsics.span)({});
(__VLS_ctx.preferences.t('firmwareSupport'));
__VLS_asFunctionalElement1(__VLS_intrinsics.input)({
    ...{ onChange: (__VLS_ctx.tool.chooseFirmware) },
    type: "file",
    accept: ".bin,application/octet-stream",
});
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.label, __VLS_intrinsics.label)({});
(__VLS_ctx.preferences.t('embeddedVersion'));
__VLS_asFunctionalElement1(__VLS_intrinsics.input)({
    maxlength: "31",
    placeholder: (__VLS_ctx.preferences.t('versionPlaceholder')),
});
(__VLS_ctx.tool.firmwareVersion.value);
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
    ...{ class: "firmware-file" },
});
/** @type {__VLS_StyleScopedClasses['firmware-file']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.span, __VLS_intrinsics.span)({});
(__VLS_ctx.preferences.t('selectedFirmware'));
__VLS_asFunctionalElement1(__VLS_intrinsics.strong, __VLS_intrinsics.strong)({});
(__VLS_ctx.tool.firmwareName.value || __VLS_ctx.preferences.t('noFile'));
__VLS_asFunctionalElement1(__VLS_intrinsics.small, __VLS_intrinsics.small)({});
(__VLS_ctx.tool.firmware.value ? `${(__VLS_ctx.tool.firmware.value.length / 1024).toFixed(1)} KiB` : '—');
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
    ...{ class: "progress-track" },
});
/** @type {__VLS_StyleScopedClasses['progress-track']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
    ...{ style: ({ width: `${__VLS_ctx.tool.ota.percent}%` }) },
});
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
    ...{ class: "progress-meta" },
});
/** @type {__VLS_StyleScopedClasses['progress-meta']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.span, __VLS_intrinsics.span)({});
(__VLS_ctx.tool.otaStateText.value);
__VLS_asFunctionalElement1(__VLS_intrinsics.span, __VLS_intrinsics.span)({});
(__VLS_ctx.tool.ota.sent);
(__VLS_ctx.tool.ota.total);
(__VLS_ctx.tool.ota.percent);
__VLS_asFunctionalElement1(__VLS_intrinsics.button, __VLS_intrinsics.button)({
    ...{ onClick: (__VLS_ctx.tool.startOta) },
    ...{ class: "button danger-button wide" },
    disabled: (!__VLS_ctx.tool.connected.value || !__VLS_ctx.tool.firmware.value || !__VLS_ctx.tool.firmwareVersion.value || __VLS_ctx.tool.busy.value),
});
/** @type {__VLS_StyleScopedClasses['button']} */ ;
/** @type {__VLS_StyleScopedClasses['danger-button']} */ ;
/** @type {__VLS_StyleScopedClasses['wide']} */ ;
(__VLS_ctx.preferences.t('startUpgrade'));
__VLS_asFunctionalElement1(__VLS_intrinsics.p, __VLS_intrinsics.p)({
    ...{ class: "warning-note" },
});
/** @type {__VLS_StyleScopedClasses['warning-note']} */ ;
(__VLS_ctx.preferences.t('otaWarning'));
// @ts-ignore
[preferences, preferences, preferences, preferences, preferences, preferences, preferences, preferences, preferences, preferences, preferences, preferences, preferences, tool, tool, tool, tool, tool, tool, tool, tool, tool, tool, tool, tool, tool, tool, tool, tool, tool, tool, tool,];
const __VLS_export = (await import('vue')).defineComponent({
    __typeProps: {},
});
export default {};
