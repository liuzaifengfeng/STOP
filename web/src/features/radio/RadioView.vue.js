import { ref } from 'vue';
import { usePreferences } from '../../app/usePreferences';
const props = defineProps();
const preferences = usePreferences();
const preparing = ref(false);
const radioHint = ref('');
async function sendPacket() {
    if (preparing.value)
        return;
    if (!props.tool.radioInput.value.trim()) {
        radioHint.value = '请先输入诊断数据';
        return;
    }
    preparing.value = true;
    try {
        if (props.tool.role === 'receiver' && props.controller.connected.value) {
            if (!await props.receiver.refresh()) {
                radioHint.value = '无法读取被控端输出状态，已取消发送';
                return;
            }
            if (props.receiver.status.value?.safetyState === 3) {
                radioHint.value = '被控端输出已接通，不能暂停安全心跳';
                return;
            }
            radioHint.value = '控制端进入 10 秒接收窗口，暂停安全心跳后发送…';
            if (!await props.controller.startRadioListen()) {
                radioHint.value = `控制端未进入接收窗口：${props.controller.notice.value}`;
                return;
            }
            await new Promise((resolve) => window.setTimeout(resolve, 500));
        }
        const sent = await props.tool.sendRadio();
        radioHint.value = sent
            ? props.tool.role === 'receiver' && props.controller.connected.value
                ? '已发送；控制端接收窗口将在 10 秒后自动关闭'
                : ''
            : props.tool.notice.value;
    }
    finally {
        preparing.value = false;
    }
}
const __VLS_ctx = {
    ...{},
    ...{},
    ...{},
    ...{},
};
let __VLS_components;
let __VLS_intrinsics;
let __VLS_directives;
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
    ...{ class: "page-stack" },
});
/** @type {__VLS_StyleScopedClasses['page-stack']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.section, __VLS_intrinsics.section)({
    ...{ class: "panel form-panel" },
});
/** @type {__VLS_StyleScopedClasses['panel']} */ ;
/** @type {__VLS_StyleScopedClasses['form-panel']} */ ;
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
(__VLS_ctx.preferences.t('radioSend'));
__VLS_asFunctionalElement1(__VLS_intrinsics.select, __VLS_intrinsics.select)({
    value: (__VLS_ctx.tool.radioMode.value),
});
__VLS_asFunctionalElement1(__VLS_intrinsics.option, __VLS_intrinsics.option)({
    value: "text",
});
(__VLS_ctx.preferences.t('text'));
__VLS_asFunctionalElement1(__VLS_intrinsics.option, __VLS_intrinsics.option)({
    value: "hex",
});
__VLS_asFunctionalElement1(__VLS_intrinsics.p, __VLS_intrinsics.p)({
    ...{ class: "section-description" },
});
/** @type {__VLS_StyleScopedClasses['section-description']} */ ;
(__VLS_ctx.preferences.t('diagnosticWarning'));
__VLS_asFunctionalElement1(__VLS_intrinsics.textarea, __VLS_intrinsics.textarea)({
    value: (__VLS_ctx.tool.radioInput.value),
    rows: "5",
    placeholder: (__VLS_ctx.tool.radioMode.value === 'hex' ? '01 A0 FF' : __VLS_ctx.preferences.t('inputDiagnostic')),
});
__VLS_asFunctionalElement1(__VLS_intrinsics.button, __VLS_intrinsics.button)({
    ...{ onClick: (__VLS_ctx.sendPacket) },
    ...{ class: "button primary" },
    disabled: (__VLS_ctx.preparing || !__VLS_ctx.tool.connected.value || !__VLS_ctx.tool.status.value?.radioReady || __VLS_ctx.tool.busy.value),
});
/** @type {__VLS_StyleScopedClasses['button']} */ ;
/** @type {__VLS_StyleScopedClasses['primary']} */ ;
(__VLS_ctx.preferences.t('sendPacket'));
if (__VLS_ctx.radioHint) {
    __VLS_asFunctionalElement1(__VLS_intrinsics.p, __VLS_intrinsics.p)({
        ...{ class: "section-description" },
    });
    /** @type {__VLS_StyleScopedClasses['section-description']} */ ;
    (__VLS_ctx.radioHint);
}
__VLS_asFunctionalElement1(__VLS_intrinsics.section, __VLS_intrinsics.section)({
    ...{ class: "panel terminal-panel" },
});
/** @type {__VLS_StyleScopedClasses['panel']} */ ;
/** @type {__VLS_StyleScopedClasses['terminal-panel']} */ ;
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
(__VLS_ctx.preferences.t('traffic'));
__VLS_asFunctionalElement1(__VLS_intrinsics.span, __VLS_intrinsics.span)({
    ...{ class: "state-badge" },
});
/** @type {__VLS_StyleScopedClasses['state-badge']} */ ;
(__VLS_ctx.tool.radioLog.value.length);
(__VLS_ctx.preferences.t('frame'));
if (__VLS_ctx.tool.radioLog.value.length === 0) {
    __VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
        ...{ class: "empty-state" },
    });
    /** @type {__VLS_StyleScopedClasses['empty-state']} */ ;
    __VLS_asFunctionalElement1(__VLS_intrinsics.strong, __VLS_intrinsics.strong)({});
    (__VLS_ctx.preferences.t('noData'));
    __VLS_asFunctionalElement1(__VLS_intrinsics.span, __VLS_intrinsics.span)({});
    (__VLS_ctx.preferences.t('noDataHint'));
}
else {
    __VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
        ...{ class: "radio-log" },
    });
    /** @type {__VLS_StyleScopedClasses['radio-log']} */ ;
    for (const [item, index] of __VLS_vFor((__VLS_ctx.tool.radioLog.value))) {
        __VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
            key: (index),
            ...{ class: "log-row" },
        });
        /** @type {__VLS_StyleScopedClasses['log-row']} */ ;
        __VLS_asFunctionalElement1(__VLS_intrinsics.b, __VLS_intrinsics.b)({
            ...{ class: (item.direction.toLowerCase()) },
        });
        (item.direction);
        __VLS_asFunctionalElement1(__VLS_intrinsics.time, __VLS_intrinsics.time)({});
        (item.time);
        __VLS_asFunctionalElement1(__VLS_intrinsics.code, __VLS_intrinsics.code)({});
        (item.value);
        // @ts-ignore
        [preferences, preferences, preferences, preferences, preferences, preferences, preferences, preferences, preferences, tool, tool, tool, tool, tool, tool, tool, tool, tool, sendPacket, preparing, radioHint, radioHint,];
    }
}
// @ts-ignore
[];
const __VLS_export = (await import('vue')).defineComponent({
    __typeProps: {},
});
export default {};
