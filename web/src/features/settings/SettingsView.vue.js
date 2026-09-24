import { usePreferences } from '../../app/usePreferences';
const preferences = usePreferences();
const sidebarOptions = ['expanded', 'auto', 'collapsed'];
const themeOptions = ['system', 'light', 'dark'];
const sidebarLabels = { expanded: 'expanded', auto: 'automatic', collapsed: 'collapsed' };
const themeLabels = { system: 'system', light: 'light', dark: 'dark' };
const __VLS_ctx = {
    ...{},
    ...{},
};
let __VLS_components;
let __VLS_intrinsics;
let __VLS_directives;
__VLS_asFunctionalElement1(__VLS_intrinsics.section, __VLS_intrinsics.section)({
    ...{ class: "settings-page" },
});
/** @type {__VLS_StyleScopedClasses['settings-page']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
    ...{ class: "settings-group" },
});
/** @type {__VLS_StyleScopedClasses['settings-group']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.h2, __VLS_intrinsics.h2)({});
(__VLS_ctx.preferences.t('sidebarBehavior'));
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
    ...{ class: "segmented-control" },
});
/** @type {__VLS_StyleScopedClasses['segmented-control']} */ ;
for (const [option] of __VLS_vFor((__VLS_ctx.sidebarOptions))) {
    __VLS_asFunctionalElement1(__VLS_intrinsics.button, __VLS_intrinsics.button)({
        ...{ onClick: (...[$event]) => {
                return (__VLS_ctx.preferences.sidebar.value = option);
                // @ts-ignore
                [preferences, preferences, sidebarOptions,];
            } },
        key: (option),
        ...{ class: ({ selected: __VLS_ctx.preferences.sidebar.value === option }) },
    });
    /** @type {__VLS_StyleScopedClasses['selected']} */ ;
    (__VLS_ctx.preferences.t(__VLS_ctx.sidebarLabels[option]));
    // @ts-ignore
    [preferences, preferences, sidebarLabels,];
}
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
    ...{ class: "settings-group" },
});
/** @type {__VLS_StyleScopedClasses['settings-group']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.h2, __VLS_intrinsics.h2)({});
(__VLS_ctx.preferences.t('theme'));
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
    ...{ class: "segmented-control" },
});
/** @type {__VLS_StyleScopedClasses['segmented-control']} */ ;
for (const [option] of __VLS_vFor((__VLS_ctx.themeOptions))) {
    __VLS_asFunctionalElement1(__VLS_intrinsics.button, __VLS_intrinsics.button)({
        ...{ onClick: (...[$event]) => {
                return (__VLS_ctx.preferences.theme.value = option);
                // @ts-ignore
                [preferences, preferences, themeOptions,];
            } },
        key: (option),
        ...{ class: ({ selected: __VLS_ctx.preferences.theme.value === option }) },
    });
    /** @type {__VLS_StyleScopedClasses['selected']} */ ;
    (__VLS_ctx.preferences.t(__VLS_ctx.themeLabels[option]));
    // @ts-ignore
    [preferences, preferences, themeLabels,];
}
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
    ...{ class: "settings-group" },
});
/** @type {__VLS_StyleScopedClasses['settings-group']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.h2, __VLS_intrinsics.h2)({});
(__VLS_ctx.preferences.t('accent'));
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
    ...{ class: "setting-select" },
});
/** @type {__VLS_StyleScopedClasses['setting-select']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.span)({
    ...{ class: "accent-dot" },
});
/** @type {__VLS_StyleScopedClasses['accent-dot']} */ ;
(__VLS_ctx.preferences.t('accentDefault'));
__VLS_asFunctionalElement1(__VLS_intrinsics.span, __VLS_intrinsics.span)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
    ...{ class: "settings-group" },
});
/** @type {__VLS_StyleScopedClasses['settings-group']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.h2, __VLS_intrinsics.h2)({});
(__VLS_ctx.preferences.t('switchLanguage'));
__VLS_asFunctionalElement1(__VLS_intrinsics.select, __VLS_intrinsics.select)({
    ...{ onChange: (...[$event]) => {
            return (__VLS_ctx.preferences.language.value = $event.target.value);
            // @ts-ignore
            [preferences, preferences, preferences, preferences,];
        } },
    ...{ class: "language-select" },
    value: (__VLS_ctx.preferences.language.value),
});
/** @type {__VLS_StyleScopedClasses['language-select']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.option, __VLS_intrinsics.option)({
    value: "zh-CN",
});
__VLS_asFunctionalElement1(__VLS_intrinsics.option, __VLS_intrinsics.option)({
    value: "en",
});
__VLS_asFunctionalElement1(__VLS_intrinsics.p, __VLS_intrinsics.p)({
    ...{ class: "settings-footnote" },
});
/** @type {__VLS_StyleScopedClasses['settings-footnote']} */ ;
(__VLS_ctx.preferences.t('savedAutomatically'));
// @ts-ignore
[preferences, preferences,];
const __VLS_export = (await import('vue')).defineComponent({});
export default {};
