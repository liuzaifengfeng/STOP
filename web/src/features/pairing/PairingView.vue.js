import { computed, ref } from 'vue';
import { textBytes } from '../../protocol';
const props = defineProps();
const testResult = ref('尚未进行无线测试');
const testing = ref(false);
const pairResult = ref('');
const pairDataReady = computed(() => props.controller.status.value?.pairFlags != null &&
    props.receiver.status.value?.pairFlags != null);
const controllerPaired = computed(() => Boolean((props.controller.status.value?.pairFlags ?? 0) & 1));
const receiverPaired = computed(() => Boolean((props.receiver.status.value?.pairFlags ?? 0) & 1));
const matched = computed(() => controllerPaired.value && receiverPaired.value &&
    props.controller.status.value?.peerMac === props.receiver.info.value?.bluetoothMac &&
    props.receiver.status.value?.peerMac === props.controller.info.value?.bluetoothMac);
const receiverOutputOn = computed(() => props.receiver.status.value?.safetyState === 3);
const heartbeatFresh = computed(() => Boolean((props.receiver.status.value?.pairFlags ?? 0) & 4));
// 网页状态仅提示条件；使能最终由固件实时检查，避免旧状态让调试入口无法点击。
const debugHint = computed(() => {
    if (!props.receiver.connected.value)
        return '请先连接被控端';
    if (props.receiver.busy.value)
        return '被控端正在处理请求，请稍候';
    if (testing.value)
        return '无线联调进行中，请等待测试完成';
    if (receiverOutputOn.value)
        return '输出已经接通，可使用断开输出';
    const status = props.receiver.status.value;
    if (status?.pairFlags == null)
        return '尚未读到完整状态，可刷新或提交使能请求';
    const reasons = [];
    if (!receiverPaired.value)
        reasons.push('未配对');
    if (!heartbeatFresh.value)
        reasons.push('安全心跳未收到或已超时');
    if (!(status.pairFlags & 2))
        reasons.push('控制端未允许输出（检查急停）');
    if (status.safetyState === 6)
        reasons.push('30 秒配对窗口尚未结束');
    if (status.safetyState === 5)
        reasons.push('硬件故障锁存');
    return reasons.length ? '当前可能阻止使能：' + reasons.join('；') + '。可点击提交，由固件确认。' : '网页条件正常；点击后由固件检查实时心跳和电源条件。';
});
const controllerSafe = computed(() => Boolean((props.controller.status.value?.pairFlags ?? 0) & 2));
async function refreshBoth() {
    await Promise.all([props.controller.refresh(), props.receiver.refresh()]);
}
async function startPairing() {
    pairResult.value = '正在请求被控端进入配对模式…';
    const ok = await props.receiver.startPairing();
    pairResult.value = ok ? '被控端已开启 30 秒配对窗口，等待控制端自动发起配对。' : `开启失败：${props.receiver.notice.value}`;
    if (ok && props.controller.connected.value)
        await props.controller.refresh();
}
async function testDirection(from, to, label, reverse = false) {
    if (testing.value || from.busy.value || to.busy.value)
        return;
    testing.value = true;
    try {
        if (!await props.receiver.refresh()) {
            testResult.value = '无法读取被控端输出状态，已取消测试';
            return;
        }
        if (receiverOutputOn.value) {
            testResult.value = '被控端输出已接通，已取消测试';
            return;
        }
        if (reverse) {
            testResult.value = '正在让控制端进入 10 秒接收窗口；被控端将保持断电…';
            if (!await props.controller.startRadioListen()) {
                testResult.value = `无法开启控制端接收窗口：${props.controller.notice.value}`;
                return;
            }
            await new Promise((resolve) => window.setTimeout(resolve, 500));
        }
        const marker = `STOP_TEST_${Date.now()}`;
        const data = textBytes(marker);
        const received = () => to.radioLog.value.some((entry) => entry.direction === 'RX' && entry.value.includes(marker));
        // E22 半双工；诊断包以间隔重试，避免一次碰撞就误判链路故障。
        for (let attempt = 1; attempt <= 3; attempt += 1) {
            testResult.value = `${label}：第 ${attempt}/3 次发送中…`;
            if (!await from.sendDiagnostic(data)) {
                testResult.value = `${label}：第 ${attempt} 次发送失败，查看发送端设备提示`;
                return;
            }
            for (let i = 0; i < 13; i += 1) {
                if (received()) {
                    testResult.value = `${label}：接收成功（第 ${attempt} 次，${marker}）`;
                    return;
                }
                await new Promise((resolve) => window.setTimeout(resolve, 100));
            }
            if (attempt < 3)
                await new Promise((resolve) => window.setTimeout(resolve, 250 + attempt * 130));
        }
        testResult.value = `${label}：3 次发送均未在接收端网页日志中看到；检查两端串口的“Diagnostic TX”和“Diagnostic RX”日志`;
    }
    finally {
        testing.value = false;
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
    ...{ class: "panel" },
});
/** @type {__VLS_StyleScopedClasses['panel']} */ ;
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
__VLS_asFunctionalElement1(__VLS_intrinsics.button, __VLS_intrinsics.button)({
    ...{ onClick: (__VLS_ctx.refreshBoth) },
    ...{ class: "button secondary" },
    disabled: (!__VLS_ctx.controller.connected.value || !__VLS_ctx.receiver.connected.value),
});
/** @type {__VLS_StyleScopedClasses['button']} */ ;
/** @type {__VLS_StyleScopedClasses['secondary']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.p, __VLS_intrinsics.p)({
    ...{ class: "section-description" },
});
/** @type {__VLS_StyleScopedClasses['section-description']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
    ...{ class: "pair-grid" },
});
/** @type {__VLS_StyleScopedClasses['pair-grid']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
    ...{ class: "pair-device" },
});
/** @type {__VLS_StyleScopedClasses['pair-device']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.b, __VLS_intrinsics.b)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.span, __VLS_intrinsics.span)({});
(__VLS_ctx.controller.connected.value ? 'BLE 已连接' : '未连接');
__VLS_asFunctionalElement1(__VLS_intrinsics.code, __VLS_intrinsics.code)({});
(__VLS_ctx.controller.info.value?.bluetoothMac ?? '—');
__VLS_asFunctionalElement1(__VLS_intrinsics.small, __VLS_intrinsics.small)({});
(__VLS_ctx.pairDataReady ? (__VLS_ctx.controllerPaired ? '已保存' : '未配对') : '待读取');
__VLS_asFunctionalElement1(__VLS_intrinsics.small, __VLS_intrinsics.small)({});
(__VLS_ctx.controller.status.value?.peerMac ?? '—');
__VLS_asFunctionalElement1(__VLS_intrinsics.small, __VLS_intrinsics.small)({});
(__VLS_ctx.pairDataReady ? (__VLS_ctx.controllerSafe ? '正常' : '断开 / 按下') : '未知');
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
    ...{ class: "pair-device" },
});
/** @type {__VLS_StyleScopedClasses['pair-device']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.b, __VLS_intrinsics.b)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.span, __VLS_intrinsics.span)({});
(__VLS_ctx.receiver.connected.value ? 'BLE 已连接' : '未连接');
__VLS_asFunctionalElement1(__VLS_intrinsics.code, __VLS_intrinsics.code)({});
(__VLS_ctx.receiver.info.value?.bluetoothMac ?? '—');
__VLS_asFunctionalElement1(__VLS_intrinsics.small, __VLS_intrinsics.small)({});
(__VLS_ctx.pairDataReady ? (__VLS_ctx.receiverPaired ? '已保存' : '未配对') : '待读取');
__VLS_asFunctionalElement1(__VLS_intrinsics.small, __VLS_intrinsics.small)({});
(__VLS_ctx.receiver.status.value?.peerMac ?? '—');
__VLS_asFunctionalElement1(__VLS_intrinsics.small, __VLS_intrinsics.small)({});
(__VLS_ctx.heartbeatFresh ? '有效' : '未收到 / 超时');
__VLS_asFunctionalElement1(__VLS_intrinsics.small, __VLS_intrinsics.small)({});
(__VLS_ctx.receiverOutputOn ? '已接通' : '断开');
__VLS_asFunctionalElement1(__VLS_intrinsics.p, __VLS_intrinsics.p)({
    ...{ class: "pair-verdict" },
    ...{ class: ({ success: __VLS_ctx.matched }) },
});
/** @type {__VLS_StyleScopedClasses['pair-verdict']} */ ;
/** @type {__VLS_StyleScopedClasses['success']} */ ;
(!__VLS_ctx.controller.connected.value || !__VLS_ctx.receiver.connected.value ? '请分别连接两台设备' : !__VLS_ctx.pairDataReady ? '设备固件尚未提供配对详情，请更新两端固件' : __VLS_ctx.matched ? '已确认两端互相配对，MAC 地址一致' : __VLS_ctx.receiverPaired && !__VLS_ctx.controllerPaired ? '被控端已保存身份，但控制端尚未收到配对确认；请重新打开被控端配对窗口' : '尚未确认两端互配，请按下方步骤操作');
__VLS_asFunctionalElement1(__VLS_intrinsics.section, __VLS_intrinsics.section)({
    ...{ class: "panel" },
});
/** @type {__VLS_StyleScopedClasses['panel']} */ ;
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
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
    ...{ class: "pair-actions" },
});
/** @type {__VLS_StyleScopedClasses['pair-actions']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.button, __VLS_intrinsics.button)({
    ...{ onClick: (__VLS_ctx.startPairing) },
    ...{ class: "button primary" },
    disabled: (!__VLS_ctx.receiver.connected.value || __VLS_ctx.receiver.busy.value || __VLS_ctx.receiverOutputOn || !__VLS_ctx.receiver.status.value?.radioReady),
});
/** @type {__VLS_StyleScopedClasses['button']} */ ;
/** @type {__VLS_StyleScopedClasses['primary']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.p, __VLS_intrinsics.p)({
    ...{ class: "section-description" },
});
/** @type {__VLS_StyleScopedClasses['section-description']} */ ;
(__VLS_ctx.pairResult || '也可用被控端本地按键进入配对模式。');
__VLS_asFunctionalElement1(__VLS_intrinsics.ol, __VLS_intrinsics.ol)({
    ...{ class: "pair-steps" },
});
/** @type {__VLS_StyleScopedClasses['pair-steps']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.li, __VLS_intrinsics.li)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.li, __VLS_intrinsics.li)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.li, __VLS_intrinsics.li)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.li, __VLS_intrinsics.li)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.p, __VLS_intrinsics.p)({
    ...{ class: "section-description" },
});
/** @type {__VLS_StyleScopedClasses['section-description']} */ ;
(__VLS_ctx.receiver.status.value?.safetyState === 6 ? '配对窗口已打开' : '配对窗口未打开');
__VLS_asFunctionalElement1(__VLS_intrinsics.section, __VLS_intrinsics.section)({
    ...{ class: "panel" },
});
/** @type {__VLS_StyleScopedClasses['panel']} */ ;
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
__VLS_asFunctionalElement1(__VLS_intrinsics.p, __VLS_intrinsics.p)({
    ...{ class: "section-description" },
});
/** @type {__VLS_StyleScopedClasses['section-description']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
    ...{ class: "pair-actions" },
});
/** @type {__VLS_StyleScopedClasses['pair-actions']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.button, __VLS_intrinsics.button)({
    ...{ onClick: (...[$event]) => {
            return (__VLS_ctx.receiver.debugOutput(true));
            // @ts-ignore
            [refreshBoth, controller, controller, controller, controller, controller, receiver, receiver, receiver, receiver, receiver, receiver, receiver, receiver, receiver, receiver, pairDataReady, pairDataReady, pairDataReady, pairDataReady, controllerPaired, controllerPaired, controllerSafe, receiverPaired, receiverPaired, heartbeatFresh, receiverOutputOn, receiverOutputOn, matched, matched, startPairing, pairResult,];
        } },
    ...{ class: "button primary" },
    disabled: (__VLS_ctx.testing || !__VLS_ctx.receiver.connected.value || __VLS_ctx.receiver.busy.value || __VLS_ctx.receiverOutputOn),
    title: (__VLS_ctx.debugHint),
});
/** @type {__VLS_StyleScopedClasses['button']} */ ;
/** @type {__VLS_StyleScopedClasses['primary']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.button, __VLS_intrinsics.button)({
    ...{ onClick: (...[$event]) => {
            return (__VLS_ctx.receiver.debugOutput(false));
            // @ts-ignore
            [receiver, receiver, receiver, receiverOutputOn, testing, debugHint,];
        } },
    ...{ class: "button secondary" },
    disabled: (!__VLS_ctx.receiver.connected.value || __VLS_ctx.receiver.busy.value),
});
/** @type {__VLS_StyleScopedClasses['button']} */ ;
/** @type {__VLS_StyleScopedClasses['secondary']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.p, __VLS_intrinsics.p)({
    ...{ class: "section-description" },
});
/** @type {__VLS_StyleScopedClasses['section-description']} */ ;
(__VLS_ctx.debugHint);
__VLS_asFunctionalElement1(__VLS_intrinsics.p, __VLS_intrinsics.p)({
    ...{ class: "pair-verdict" },
});
/** @type {__VLS_StyleScopedClasses['pair-verdict']} */ ;
(__VLS_ctx.receiver.notice.value);
(!__VLS_ctx.receiver.connected.value || __VLS_ctx.receiver.status.value?.pairFlags == null ? '未知，请连接并刷新' : __VLS_ctx.receiverOutputOn ? '已接通' : '断开');
__VLS_asFunctionalElement1(__VLS_intrinsics.p, __VLS_intrinsics.p)({
    ...{ class: "section-description" },
});
/** @type {__VLS_StyleScopedClasses['section-description']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.section, __VLS_intrinsics.section)({
    ...{ class: "panel" },
});
/** @type {__VLS_StyleScopedClasses['panel']} */ ;
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
__VLS_asFunctionalElement1(__VLS_intrinsics.p, __VLS_intrinsics.p)({
    ...{ class: "section-description" },
});
/** @type {__VLS_StyleScopedClasses['section-description']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
    ...{ class: "pair-actions" },
});
/** @type {__VLS_StyleScopedClasses['pair-actions']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.button, __VLS_intrinsics.button)({
    ...{ onClick: (...[$event]) => {
            return (__VLS_ctx.testDirection(__VLS_ctx.controller, __VLS_ctx.receiver, '控制端 → 被控端'));
            // @ts-ignore
            [controller, receiver, receiver, receiver, receiver, receiver, receiver, receiverOutputOn, debugHint, testDirection,];
        } },
    ...{ class: "button primary" },
    disabled: (__VLS_ctx.testing || !__VLS_ctx.controller.connected.value || !__VLS_ctx.receiver.connected.value || __VLS_ctx.receiverOutputOn || __VLS_ctx.controller.busy.value || __VLS_ctx.receiver.busy.value),
});
/** @type {__VLS_StyleScopedClasses['button']} */ ;
/** @type {__VLS_StyleScopedClasses['primary']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.button, __VLS_intrinsics.button)({
    ...{ onClick: (...[$event]) => {
            return (__VLS_ctx.testDirection(__VLS_ctx.receiver, __VLS_ctx.controller, '被控端 → 控制端', true));
            // @ts-ignore
            [controller, controller, controller, receiver, receiver, receiver, receiverOutputOn, testing, testDirection,];
        } },
    ...{ class: "button secondary" },
    disabled: (__VLS_ctx.testing || !__VLS_ctx.controller.connected.value || !__VLS_ctx.receiver.connected.value || __VLS_ctx.receiverOutputOn || __VLS_ctx.controller.busy.value || __VLS_ctx.receiver.busy.value),
});
/** @type {__VLS_StyleScopedClasses['button']} */ ;
/** @type {__VLS_StyleScopedClasses['secondary']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.p, __VLS_intrinsics.p)({
    ...{ class: "pair-verdict" },
});
/** @type {__VLS_StyleScopedClasses['pair-verdict']} */ ;
(__VLS_ctx.testResult);
__VLS_asFunctionalElement1(__VLS_intrinsics.p, __VLS_intrinsics.p)({
    ...{ class: "section-description" },
});
/** @type {__VLS_StyleScopedClasses['section-description']} */ ;
(__VLS_ctx.heartbeatFresh ? '有效' : '未确认');
(__VLS_ctx.matched ? 'MAC 配对一致' : 'MAC 配对未确认');
(__VLS_ctx.receiverOutputOn ? '接通' : '断开');
// @ts-ignore
[controller, controller, receiver, receiver, heartbeatFresh, receiverOutputOn, receiverOutputOn, matched, testing, testResult,];
const __VLS_export = (await import('vue')).defineComponent({
    __typeProps: {},
});
export default {};
