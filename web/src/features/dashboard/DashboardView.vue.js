import { usePreferences } from '../../app/usePreferences';
const __VLS_props = defineProps();
const preferences = usePreferences();
const receiverStates = {
    1: '未配对', 2: '断电待确认', 3: '输出已接通',
    4: '无线失联', 5: '硬件故障', 6: '配对中',
};
const receiverState = (state) => receiverStates[state ?? 0] ?? '状态未知';
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
    ...{ class: "device-hero panel" },
});
/** @type {__VLS_StyleScopedClasses['device-hero']} */ ;
/** @type {__VLS_StyleScopedClasses['panel']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
    ...{ class: "device-identity" },
});
/** @type {__VLS_StyleScopedClasses['device-identity']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.span, __VLS_intrinsics.span)({
    ...{ class: "section-kicker" },
});
/** @type {__VLS_StyleScopedClasses['section-kicker']} */ ;
(__VLS_ctx.preferences.t('connectedDevice'));
__VLS_asFunctionalElement1(__VLS_intrinsics.h2, __VLS_intrinsics.h2)({});
(__VLS_ctx.tool.config.alias);
__VLS_asFunctionalElement1(__VLS_intrinsics.code, __VLS_intrinsics.code)({});
(__VLS_ctx.tool.info.value?.bluetoothMac ?? __VLS_ctx.preferences.t('waitingIdentity'));
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
    ...{ class: "chip-row" },
});
/** @type {__VLS_StyleScopedClasses['chip-row']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.span, __VLS_intrinsics.span)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.span, __VLS_intrinsics.span)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.span, __VLS_intrinsics.span)({});
(__VLS_ctx.tool.status.value?.radioMode === 6 ? 'E22 + ESP-NOW' : __VLS_ctx.tool.status.value?.radioMode === 5 ? 'ESP-NOW' : 'E22');
(__VLS_ctx.tool.status.value?.radioReady ? 'READY' : 'OFFLINE');
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
    ...{ class: "device-visual" },
    'aria-hidden': "true",
});
/** @type {__VLS_StyleScopedClasses['device-visual']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.div)({
    ...{ class: "antenna" },
});
/** @type {__VLS_StyleScopedClasses['antenna']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
    ...{ class: "device-body" },
});
/** @type {__VLS_StyleScopedClasses['device-body']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.i)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.span, __VLS_intrinsics.span)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.small, __VLS_intrinsics.small)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.button, __VLS_intrinsics.button)({
    ...{ onClick: (__VLS_ctx.tool.refresh) },
    ...{ class: "button secondary refresh-button" },
    disabled: (!__VLS_ctx.tool.connected.value || __VLS_ctx.tool.busy.value),
});
/** @type {__VLS_StyleScopedClasses['button']} */ ;
/** @type {__VLS_StyleScopedClasses['secondary']} */ ;
/** @type {__VLS_StyleScopedClasses['refresh-button']} */ ;
(__VLS_ctx.preferences.t('refresh'));
__VLS_asFunctionalElement1(__VLS_intrinsics.section, __VLS_intrinsics.section)({
    ...{ class: "metric-grid" },
});
/** @type {__VLS_StyleScopedClasses['metric-grid']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.article, __VLS_intrinsics.article)({
    ...{ class: "metric-card danger" },
});
/** @type {__VLS_StyleScopedClasses['metric-card']} */ ;
/** @type {__VLS_StyleScopedClasses['danger']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.span, __VLS_intrinsics.span)({});
(__VLS_ctx.preferences.t('safetyStatus'));
__VLS_asFunctionalElement1(__VLS_intrinsics.strong, __VLS_intrinsics.strong)({});
(__VLS_ctx.tool.info.value?.productId === 2 ? __VLS_ctx.receiverState(__VLS_ctx.tool.status.value?.safetyState) : __VLS_ctx.tool.status.value?.pairFlags == null ? '状态未知' : (__VLS_ctx.tool.status.value.pairFlags & 2) ? '急停已释放' : '急停按下 / 断开');
__VLS_asFunctionalElement1(__VLS_intrinsics.small, __VLS_intrinsics.small)({});
(__VLS_ctx.tool.info.value?.productId === 2 ? '上电后收到控制端释放状态和连续安全心跳，自动供电' : '以 GPIO6 常闭触点实测状态为准');
if (__VLS_ctx.tool.info.value?.productId === 2) {
    __VLS_asFunctionalElement1(__VLS_intrinsics.article, __VLS_intrinsics.article)({
        ...{ class: "metric-card" },
    });
    /** @type {__VLS_StyleScopedClasses['metric-card']} */ ;
    __VLS_asFunctionalElement1(__VLS_intrinsics.span, __VLS_intrinsics.span)({});
    __VLS_asFunctionalElement1(__VLS_intrinsics.strong, __VLS_intrinsics.strong)({});
    (__VLS_ctx.tool.status.value?.receiverVinDisabled ? '已禁用' : __VLS_ctx.tool.status.value?.receiverVinMv == null ? '—' : `${(__VLS_ctx.tool.status.value.receiverVinMv / 1000).toFixed(2)} V`);
    __VLS_asFunctionalElement1(__VLS_intrinsics.small, __VLS_intrinsics.small)({});
    (__VLS_ctx.tool.status.value?.receiverVinDisabled ? 'ADC 异常，输入电压保护临时禁用' : 'GPIO0 分压测量');
}
else {
    __VLS_asFunctionalElement1(__VLS_intrinsics.article, __VLS_intrinsics.article)({
        ...{ class: "metric-card" },
    });
    /** @type {__VLS_StyleScopedClasses['metric-card']} */ ;
    __VLS_asFunctionalElement1(__VLS_intrinsics.span, __VLS_intrinsics.span)({});
    (__VLS_ctx.preferences.t('battery'));
    __VLS_asFunctionalElement1(__VLS_intrinsics.strong, __VLS_intrinsics.strong)({});
    (__VLS_ctx.tool.batteryText.value);
    __VLS_asFunctionalElement1(__VLS_intrinsics.small, __VLS_intrinsics.small)({});
    (__VLS_ctx.tool.status.value ? `${__VLS_ctx.tool.status.value.batteryMv} mV` : __VLS_ctx.preferences.t('waitingStatus'));
}
if (__VLS_ctx.tool.info.value?.productId === 2) {
    __VLS_asFunctionalElement1(__VLS_intrinsics.article, __VLS_intrinsics.article)({
        ...{ class: "metric-card" },
    });
    /** @type {__VLS_StyleScopedClasses['metric-card']} */ ;
    __VLS_asFunctionalElement1(__VLS_intrinsics.span, __VLS_intrinsics.span)({});
    __VLS_asFunctionalElement1(__VLS_intrinsics.strong, __VLS_intrinsics.strong)({});
    (__VLS_ctx.tool.status.value ? `${(__VLS_ctx.tool.status.value.batteryMv / 1000).toFixed(2)} V` : '—');
    __VLS_asFunctionalElement1(__VLS_intrinsics.small, __VLS_intrinsics.small)({});
}
if (__VLS_ctx.tool.info.value?.productId === 2) {
    __VLS_asFunctionalElement1(__VLS_intrinsics.article, __VLS_intrinsics.article)({
        ...{ class: "metric-card" },
    });
    /** @type {__VLS_StyleScopedClasses['metric-card']} */ ;
    __VLS_asFunctionalElement1(__VLS_intrinsics.span, __VLS_intrinsics.span)({});
    __VLS_asFunctionalElement1(__VLS_intrinsics.strong, __VLS_intrinsics.strong)({});
    (__VLS_ctx.tool.status.value?.receiverInaValid ? `${(__VLS_ctx.tool.status.value.receiverCurrentMa / 1000).toFixed(2)} A` : '—');
    __VLS_asFunctionalElement1(__VLS_intrinsics.small, __VLS_intrinsics.small)({});
    (__VLS_ctx.tool.status.value?.receiverOverCurrent ? '过流告警' : 'INA226 分流电阻测量');
}
__VLS_asFunctionalElement1(__VLS_intrinsics.article, __VLS_intrinsics.article)({
    ...{ class: "metric-card" },
});
/** @type {__VLS_StyleScopedClasses['metric-card']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.span, __VLS_intrinsics.span)({});
(__VLS_ctx.preferences.t('uptime'));
__VLS_asFunctionalElement1(__VLS_intrinsics.strong, __VLS_intrinsics.strong)({});
(__VLS_ctx.tool.status.value ? `${__VLS_ctx.tool.status.value.uptimeSeconds}s` : '—');
__VLS_asFunctionalElement1(__VLS_intrinsics.small, __VLS_intrinsics.small)({});
(__VLS_ctx.preferences.t('uptimeHint'));
__VLS_asFunctionalElement1(__VLS_intrinsics.article, __VLS_intrinsics.article)({
    ...{ class: "metric-card" },
});
/** @type {__VLS_StyleScopedClasses['metric-card']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.span, __VLS_intrinsics.span)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.strong, __VLS_intrinsics.strong)({});
(__VLS_ctx.tool.status.value?.mtu ?? '—');
__VLS_asFunctionalElement1(__VLS_intrinsics.small, __VLS_intrinsics.small)({});
(__VLS_ctx.preferences.t('currentValue'));
__VLS_asFunctionalElement1(__VLS_intrinsics.section, __VLS_intrinsics.section)({
    ...{ class: "panel details-panel" },
});
/** @type {__VLS_StyleScopedClasses['panel']} */ ;
/** @type {__VLS_StyleScopedClasses['details-panel']} */ ;
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
__VLS_asFunctionalElement1(__VLS_intrinsics.span, __VLS_intrinsics.span)({
    ...{ class: "state-badge" },
});
/** @type {__VLS_StyleScopedClasses['state-badge']} */ ;
(__VLS_ctx.tool.status.value?.pairFlags == null ? '旧版状态' : (__VLS_ctx.tool.status.value.pairFlags & 1) ? '已配对' : '未配对');
__VLS_asFunctionalElement1(__VLS_intrinsics.dl, __VLS_intrinsics.dl)({
    ...{ class: "detail-list" },
});
/** @type {__VLS_StyleScopedClasses['detail-list']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.dt, __VLS_intrinsics.dt)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.dd, __VLS_intrinsics.dd)({});
(__VLS_ctx.tool.info.value?.bluetoothMac ?? '—');
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.dt, __VLS_intrinsics.dt)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.dd, __VLS_intrinsics.dd)({});
(__VLS_ctx.tool.status.value?.peerMac ?? '—');
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.dt, __VLS_intrinsics.dt)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.dd, __VLS_intrinsics.dd)({});
(__VLS_ctx.tool.info.value?.productId === 2 ? ((__VLS_ctx.tool.status.value?.pairFlags ?? 0) & 4 ? '有效' : '未收到 / 超时') : '在配对与联调页查看');
__VLS_asFunctionalElement1(__VLS_intrinsics.section, __VLS_intrinsics.section)({
    ...{ class: "panel details-panel" },
});
/** @type {__VLS_StyleScopedClasses['panel']} */ ;
/** @type {__VLS_StyleScopedClasses['details-panel']} */ ;
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
(__VLS_ctx.preferences.t('deviceInfo'));
__VLS_asFunctionalElement1(__VLS_intrinsics.span, __VLS_intrinsics.span)({
    ...{ class: "state-badge" },
});
/** @type {__VLS_StyleScopedClasses['state-badge']} */ ;
(__VLS_ctx.tool.connected.value ? 'ONLINE' : 'OFFLINE');
__VLS_asFunctionalElement1(__VLS_intrinsics.dl, __VLS_intrinsics.dl)({
    ...{ class: "detail-list" },
});
/** @type {__VLS_StyleScopedClasses['detail-list']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.dt, __VLS_intrinsics.dt)({});
(__VLS_ctx.preferences.t('firmwareVersion'));
__VLS_asFunctionalElement1(__VLS_intrinsics.dd, __VLS_intrinsics.dd)({});
(__VLS_ctx.tool.info.value?.firmwareVersion ?? '—');
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.dt, __VLS_intrinsics.dt)({});
(__VLS_ctx.preferences.t('productId'));
__VLS_asFunctionalElement1(__VLS_intrinsics.dd, __VLS_intrinsics.dd)({});
(__VLS_ctx.tool.info.value?.productId ?? '—');
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.dt, __VLS_intrinsics.dt)({});
(__VLS_ctx.preferences.t('hardwareRevision'));
__VLS_asFunctionalElement1(__VLS_intrinsics.dd, __VLS_intrinsics.dd)({});
(__VLS_ctx.tool.info.value?.hardwareRevision ?? '—');
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.dt, __VLS_intrinsics.dt)({});
(__VLS_ctx.preferences.t('protocolVersion'));
__VLS_asFunctionalElement1(__VLS_intrinsics.dd, __VLS_intrinsics.dd)({});
(__VLS_ctx.tool.info.value?.protocolVersion ?? '—');
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.dt, __VLS_intrinsics.dt)({});
(__VLS_ctx.preferences.t('radioModule'));
__VLS_asFunctionalElement1(__VLS_intrinsics.dd, __VLS_intrinsics.dd)({});
(__VLS_ctx.tool.status.value?.radioReady ? __VLS_ctx.preferences.t('available') : __VLS_ctx.preferences.t('notAvailable'));
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.dt, __VLS_intrinsics.dt)({});
(__VLS_ctx.preferences.t('otaStatus'));
__VLS_asFunctionalElement1(__VLS_intrinsics.dd, __VLS_intrinsics.dd)({});
(__VLS_ctx.tool.status.value?.otaState ?? 0);
// @ts-ignore
[preferences, preferences, preferences, preferences, preferences, preferences, preferences, preferences, preferences, preferences, preferences, preferences, preferences, preferences, preferences, preferences, preferences, preferences, tool, tool, tool, tool, tool, tool, tool, tool, tool, tool, tool, tool, tool, tool, tool, tool, tool, tool, tool, tool, tool, tool, tool, tool, tool, tool, tool, tool, tool, tool, tool, tool, tool, tool, tool, tool, tool, tool, tool, tool, tool, tool, tool, tool, receiverState,];
const __VLS_export = (await import('vue')).defineComponent({
    __typeProps: {},
});
export default {};
