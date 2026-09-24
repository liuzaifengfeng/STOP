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
    ...{ class: "panel form-panel narrow-panel" },
});
/** @type {__VLS_StyleScopedClasses['panel']} */ ;
/** @type {__VLS_StyleScopedClasses['form-panel']} */ ;
/** @type {__VLS_StyleScopedClasses['narrow-panel']} */ ;
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
(__VLS_ctx.preferences.t('basicParameters'));
__VLS_asFunctionalElement1(__VLS_intrinsics.span, __VLS_intrinsics.span)({
    ...{ class: "state-badge" },
});
/** @type {__VLS_StyleScopedClasses['state-badge']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.p, __VLS_intrinsics.p)({
    ...{ class: "section-description" },
});
/** @type {__VLS_StyleScopedClasses['section-description']} */ ;
(__VLS_ctx.preferences.t('storedNvs'));
__VLS_asFunctionalElement1(__VLS_intrinsics.label, __VLS_intrinsics.label)({});
(__VLS_ctx.preferences.t('alias'));
__VLS_asFunctionalElement1(__VLS_intrinsics.input)({
    maxlength: "31",
});
(__VLS_ctx.tool.config.alias);
__VLS_asFunctionalElement1(__VLS_intrinsics.label, __VLS_intrinsics.label)({});
(__VLS_ctx.preferences.t('statusPeriod'));
__VLS_asFunctionalElement1(__VLS_intrinsics.input)({
    type: "number",
    min: "1000",
    max: "60000",
    step: "1000",
});
(__VLS_ctx.tool.config.statusPeriodMs);
__VLS_asFunctionalElement1(__VLS_intrinsics.label, __VLS_intrinsics.label)({});
(__VLS_ctx.preferences.t('radioTimeout'));
__VLS_asFunctionalElement1(__VLS_intrinsics.input)({
    type: "number",
    min: "100",
    max: "5000",
    step: "100",
});
(__VLS_ctx.tool.config.radioTxTimeoutMs);
__VLS_asFunctionalElement1(__VLS_intrinsics.button, __VLS_intrinsics.button)({
    ...{ onClick: (__VLS_ctx.tool.saveConfig) },
    ...{ class: "button primary wide" },
    disabled: (!__VLS_ctx.tool.connected.value || __VLS_ctx.tool.busy.value),
});
/** @type {__VLS_StyleScopedClasses['button']} */ ;
/** @type {__VLS_StyleScopedClasses['primary']} */ ;
/** @type {__VLS_StyleScopedClasses['wide']} */ ;
(__VLS_ctx.preferences.t('saveDevice'));
// @ts-ignore
[preferences, preferences, preferences, preferences, preferences, preferences, tool, tool, tool, tool, tool, tool,];
const __VLS_export = (await import('vue')).defineComponent({
    __typeProps: {},
});
export default {};
