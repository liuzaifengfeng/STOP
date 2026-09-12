import { computed, onUnmounted, reactive, ref } from 'vue';
import { StopDeviceClient } from './device-client';
import { bytesText, bytesToHex, FrameFlag, hexToBytes, MessageType, parseDeviceStatus, textBytes } from './protocol';
const client = new StopDeviceClient();
const connected = ref(false), busy = ref(false), notice = ref('等待连接设备');
const info = ref(), status = ref(), config = reactive({ alias: 'STOP-C6', statusPeriodMs: 5000, radioTxTimeoutMs: 1000 });
const radioMode = ref('text'), radioInput = ref(''), radioLog = ref([]);
const firmware = ref(), firmwareName = ref(''), firmwareVersion = ref(''), firmwareUrl = ref('');
const ota = reactive({ sent: 0, total: 0, percent: 0, state: 0 });
const unsubscribe = client.onFrame((frame) => {
    if (frame.type === MessageType.Error && frame.payload.length === 0) {
        connected.value = false;
        notice.value = '设备已断开';
        return;
    }
    if (frame.type === MessageType.DeviceStatusEvent && frame.payload.length === 20)
        status.value = parseDeviceStatus(frame.payload);
    if (frame.type === MessageType.RadioRxEvent && (frame.flags & FrameFlag.Event) && frame.payload.length >= 2) {
        const length = new DataView(frame.payload.buffer, frame.payload.byteOffset).getUint16(0, true);
        const data = frame.payload.subarray(2, 2 + length);
        radioLog.value.unshift({ time: new Date().toLocaleTimeString(), direction: 'RX', value: `${bytesToHex(data)}  ·  ${safeText(data)}` });
    }
});
onUnmounted(unsubscribe);
const batteryText = computed(() => status.value?.batterySoc == null ? '—' : `${status.value.batterySoc}%`);
const otaStateText = computed(() => ['空闲', '准备', '接收中', '校验中', '等待重启', '失败', '已取消'][ota.state] ?? `状态 ${ota.state}`);
async function run(label, operation) {
    busy.value = true;
    notice.value = label;
    try {
        const result = await operation();
        notice.value = `${label}完成`;
        return result;
    }
    catch (error) {
        notice.value = error instanceof Error ? error.message : String(error);
        return undefined;
    }
    finally {
        busy.value = false;
    }
}
async function connect() {
    await run('连接设备', async () => {
        await client.connect();
        connected.value = true;
        const results = await Promise.allSettled([client.getInfo(), client.getStatus(), client.getConfig()]);
        if (results[0].status === 'fulfilled')
            info.value = results[0].value;
        if (results[1].status === 'fulfilled')
            status.value = results[1].value;
        if (results[2].status === 'fulfilled')
            Object.assign(config, results[2].value);
        const failures = results.filter((result) => result.status === 'rejected');
        if (failures.length)
            throw new Error(`设备已连接，但有 ${failures.length} 项初始化读取失败`);
    });
}
function disconnect() { client.disconnect(); connected.value = false; notice.value = '已主动断开'; }
async function refresh() { const value = await run('刷新状态', () => client.getStatus()); if (value)
    status.value = value; }
async function saveConfig() { const value = await run('保存参数', () => client.setConfig({ ...config })); if (value)
    Object.assign(config, value); }
async function sendRadio() {
    const data = radioMode.value === 'hex' ? hexToBytes(radioInput.value) : textBytes(radioInput.value);
    const sent = await run('发送 E22 诊断包', () => client.radioSend(data));
    if (sent)
        radioLog.value.unshift({ time: new Date().toLocaleTimeString(), direction: 'TX', value: `${bytesToHex(data)}  ·  ${safeText(data)}` });
}
function safeText(data) { const text = bytesText(data); return [...text].every((char) => char >= ' ' && char !== '\u007f') ? text : '[二进制]'; }
async function chooseFirmware(event) {
    const file = event.target.files?.[0];
    if (!file)
        return;
    setFirmware(new Uint8Array(await file.arrayBuffer()), file.name);
}
async function loadFirmwareUrl() {
    const value = await run('从云端下载固件', async () => {
        const response = await fetch(firmwareUrl.value, { cache: 'no-store' });
        if (!response.ok)
            throw new Error(`固件下载失败：HTTP ${response.status}`);
        return new Uint8Array(await response.arrayBuffer());
    });
    if (value)
        setFirmware(value, firmwareUrl.value.split('/').pop() || 'cloud-firmware.bin');
}
function setFirmware(image, name) {
    firmware.value = image;
    firmwareName.value = name;
    ota.total = image.length;
    if (image.length < 80) {
        notice.value = '固件文件太短，无法读取 ESP 应用描述';
        return;
    }
    const view = new DataView(image.buffer, image.byteOffset, image.byteLength);
    if (view.getUint32(32, true) !== 0xabcd5432) {
        notice.value = '未找到 ESP 应用描述，请确认选择的是 app firmware.bin';
        return;
    }
    const versionBytes = image.subarray(48, 80);
    const end = versionBytes.indexOf(0);
    firmwareVersion.value = bytesText(end >= 0 ? versionBytes.subarray(0, end) : versionBytes).trim();
    notice.value = `已读取固件版本 ${firmwareVersion.value}`;
}
async function startOta() {
    if (!firmware.value) {
        notice.value = '请先选择或下载固件';
        return;
    }
    ota.sent = 0;
    ota.percent = 0;
    ota.state = 1;
    await run('执行 BLE OTA', () => client.otaUpdate(firmware.value, firmwareVersion.value, (progress) => Object.assign(ota, progress)));
}
const __VLS_ctx = {
    ...{},
    ...{},
};
let __VLS_components;
let __VLS_intrinsics;
let __VLS_directives;
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
    ...{ class: "shell" },
});
/** @type {__VLS_StyleScopedClasses['shell']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.header, __VLS_intrinsics.header)({
    ...{ class: "topbar" },
});
/** @type {__VLS_StyleScopedClasses['topbar']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.span, __VLS_intrinsics.span)({
    ...{ class: "eyebrow" },
});
/** @type {__VLS_StyleScopedClasses['eyebrow']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.h1, __VLS_intrinsics.h1)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
    ...{ class: "connection" },
});
/** @type {__VLS_StyleScopedClasses['connection']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.span)({
    ...{ class: (['dot', { online: __VLS_ctx.connected }]) },
});
/** @type {__VLS_StyleScopedClasses['online']} */ ;
/** @type {__VLS_StyleScopedClasses['dot']} */ ;
(__VLS_ctx.connected ? `已连接 ${__VLS_ctx.client.name}` : '未连接');
if (!__VLS_ctx.connected) {
    __VLS_asFunctionalElement1(__VLS_intrinsics.button, __VLS_intrinsics.button)({
        ...{ onClick: (__VLS_ctx.connect) },
        ...{ class: "primary" },
        disabled: (__VLS_ctx.busy),
    });
    /** @type {__VLS_StyleScopedClasses['primary']} */ ;
}
else {
    __VLS_asFunctionalElement1(__VLS_intrinsics.button, __VLS_intrinsics.button)({
        ...{ onClick: (__VLS_ctx.disconnect) },
        ...{ class: "ghost" },
    });
    /** @type {__VLS_StyleScopedClasses['ghost']} */ ;
}
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
    ...{ class: "safety-banner" },
});
/** @type {__VLS_StyleScopedClasses['safety-banner']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.strong, __VLS_intrinsics.strong)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.span, __VLS_intrinsics.span)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.p, __VLS_intrinsics.p)({
    ...{ class: "notice" },
});
/** @type {__VLS_StyleScopedClasses['notice']} */ ;
(__VLS_ctx.notice);
__VLS_asFunctionalElement1(__VLS_intrinsics.main, __VLS_intrinsics.main)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.section, __VLS_intrinsics.section)({
    ...{ class: "hero-grid" },
});
/** @type {__VLS_StyleScopedClasses['hero-grid']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.article, __VLS_intrinsics.article)({
    ...{ class: "card identity" },
});
/** @type {__VLS_StyleScopedClasses['card']} */ ;
/** @type {__VLS_StyleScopedClasses['identity']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
    ...{ class: "card-head" },
});
/** @type {__VLS_StyleScopedClasses['card-head']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.h2, __VLS_intrinsics.h2)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.button, __VLS_intrinsics.button)({
    ...{ onClick: (__VLS_ctx.refresh) },
    ...{ class: "icon-button" },
    disabled: (!__VLS_ctx.connected || __VLS_ctx.busy),
});
/** @type {__VLS_StyleScopedClasses['icon-button']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
    ...{ class: "identity-name" },
});
/** @type {__VLS_StyleScopedClasses['identity-name']} */ ;
(__VLS_ctx.config.alias);
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
    ...{ class: "muted" },
});
/** @type {__VLS_StyleScopedClasses['muted']} */ ;
(__VLS_ctx.info?.bluetoothMac ?? '等待读取设备标识');
__VLS_asFunctionalElement1(__VLS_intrinsics.dl, __VLS_intrinsics.dl)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.dt, __VLS_intrinsics.dt)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.dd, __VLS_intrinsics.dd)({});
(__VLS_ctx.info?.firmwareVersion ?? '—');
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.dt, __VLS_intrinsics.dt)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.dd, __VLS_intrinsics.dd)({});
(__VLS_ctx.info ? `${__VLS_ctx.info.productId} / ${__VLS_ctx.info.hardwareRevision}` : '—');
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.dt, __VLS_intrinsics.dt)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.dd, __VLS_intrinsics.dd)({});
(__VLS_ctx.status?.mtu ?? '—');
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.dt, __VLS_intrinsics.dt)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.dd, __VLS_intrinsics.dd)({});
(__VLS_ctx.status?.radioReady ? '可用' : '不可用');
__VLS_asFunctionalElement1(__VLS_intrinsics.article, __VLS_intrinsics.article)({
    ...{ class: "metric red" },
});
/** @type {__VLS_StyleScopedClasses['metric']} */ ;
/** @type {__VLS_StyleScopedClasses['red']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.span, __VLS_intrinsics.span)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.strong, __VLS_intrinsics.strong)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.small, __VLS_intrinsics.small)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.article, __VLS_intrinsics.article)({
    ...{ class: "metric" },
});
/** @type {__VLS_StyleScopedClasses['metric']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.span, __VLS_intrinsics.span)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.strong, __VLS_intrinsics.strong)({});
(__VLS_ctx.batteryText);
__VLS_asFunctionalElement1(__VLS_intrinsics.small, __VLS_intrinsics.small)({});
(__VLS_ctx.status ? `${__VLS_ctx.status.batteryMv} mV` : '等待设备状态');
__VLS_asFunctionalElement1(__VLS_intrinsics.article, __VLS_intrinsics.article)({
    ...{ class: "metric" },
});
/** @type {__VLS_StyleScopedClasses['metric']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.span, __VLS_intrinsics.span)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.strong, __VLS_intrinsics.strong)({});
(__VLS_ctx.status ? `${__VLS_ctx.status.uptimeSeconds}s` : '—');
__VLS_asFunctionalElement1(__VLS_intrinsics.small, __VLS_intrinsics.small)({});
(__VLS_ctx.status?.otaState ?? 0);
__VLS_asFunctionalElement1(__VLS_intrinsics.section, __VLS_intrinsics.section)({
    ...{ class: "two-column" },
});
/** @type {__VLS_StyleScopedClasses['two-column']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.article, __VLS_intrinsics.article)({
    ...{ class: "card" },
});
/** @type {__VLS_StyleScopedClasses['card']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
    ...{ class: "card-head" },
});
/** @type {__VLS_StyleScopedClasses['card-head']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.span, __VLS_intrinsics.span)({
    ...{ class: "section-number" },
});
/** @type {__VLS_StyleScopedClasses['section-number']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.h2, __VLS_intrinsics.h2)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.label, __VLS_intrinsics.label)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.input)({
    maxlength: "31",
});
(__VLS_ctx.config.alias);
__VLS_asFunctionalElement1(__VLS_intrinsics.label, __VLS_intrinsics.label)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.input)({
    type: "number",
    min: "1000",
    max: "60000",
    step: "1000",
});
(__VLS_ctx.config.statusPeriodMs);
__VLS_asFunctionalElement1(__VLS_intrinsics.label, __VLS_intrinsics.label)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.input)({
    type: "number",
    min: "100",
    max: "5000",
    step: "100",
});
(__VLS_ctx.config.radioTxTimeoutMs);
__VLS_asFunctionalElement1(__VLS_intrinsics.button, __VLS_intrinsics.button)({
    ...{ onClick: (__VLS_ctx.saveConfig) },
    ...{ class: "primary full" },
    disabled: (!__VLS_ctx.connected || __VLS_ctx.busy),
});
/** @type {__VLS_StyleScopedClasses['primary']} */ ;
/** @type {__VLS_StyleScopedClasses['full']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.article, __VLS_intrinsics.article)({
    ...{ class: "card" },
});
/** @type {__VLS_StyleScopedClasses['card']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
    ...{ class: "card-head" },
});
/** @type {__VLS_StyleScopedClasses['card-head']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.span, __VLS_intrinsics.span)({
    ...{ class: "section-number" },
});
/** @type {__VLS_StyleScopedClasses['section-number']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.h2, __VLS_intrinsics.h2)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.select, __VLS_intrinsics.select)({
    value: (__VLS_ctx.radioMode),
});
__VLS_asFunctionalElement1(__VLS_intrinsics.option, __VLS_intrinsics.option)({
    value: "text",
});
__VLS_asFunctionalElement1(__VLS_intrinsics.option, __VLS_intrinsics.option)({
    value: "hex",
});
__VLS_asFunctionalElement1(__VLS_intrinsics.textarea)({
    value: (__VLS_ctx.radioInput),
    rows: "4",
    placeholder: (__VLS_ctx.radioMode === 'hex' ? '例：01 A0 FF' : '输入诊断数据，不得作为安全控制报文'),
});
__VLS_asFunctionalElement1(__VLS_intrinsics.button, __VLS_intrinsics.button)({
    ...{ onClick: (__VLS_ctx.sendRadio) },
    ...{ class: "primary" },
    disabled: (!__VLS_ctx.connected || !__VLS_ctx.status?.radioReady || __VLS_ctx.busy),
});
/** @type {__VLS_StyleScopedClasses['primary']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
    ...{ class: "radio-log" },
});
/** @type {__VLS_StyleScopedClasses['radio-log']} */ ;
if (__VLS_ctx.radioLog.length === 0) {
    __VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
        ...{ class: "empty" },
    });
    /** @type {__VLS_StyleScopedClasses['empty']} */ ;
}
for (const [item, index] of __VLS_vFor((__VLS_ctx.radioLog))) {
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
    [connected, connected, connected, connected, connected, connected, client, connect, busy, busy, busy, busy, disconnect, notice, refresh, config, config, config, config, info, info, info, info, info, status, status, status, status, status, status, status, status, batteryText, saveConfig, radioMode, radioMode, radioInput, sendRadio, radioLog, radioLog,];
}
__VLS_asFunctionalElement1(__VLS_intrinsics.section, __VLS_intrinsics.section)({
    ...{ class: "card ota-card" },
});
/** @type {__VLS_StyleScopedClasses['card']} */ ;
/** @type {__VLS_StyleScopedClasses['ota-card']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
    ...{ class: "card-head" },
});
/** @type {__VLS_StyleScopedClasses['card-head']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.span, __VLS_intrinsics.span)({
    ...{ class: "section-number" },
});
/** @type {__VLS_StyleScopedClasses['section-number']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.h2, __VLS_intrinsics.h2)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.span, __VLS_intrinsics.span)({
    ...{ class: "tag" },
});
/** @type {__VLS_StyleScopedClasses['tag']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
    ...{ class: "ota-grid" },
});
/** @type {__VLS_StyleScopedClasses['ota-grid']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.label, __VLS_intrinsics.label)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
    ...{ class: "inline" },
});
/** @type {__VLS_StyleScopedClasses['inline']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.input)({
    type: "url",
    placeholder: "https://example.com/firmware.bin",
});
(__VLS_ctx.firmwareUrl);
__VLS_asFunctionalElement1(__VLS_intrinsics.button, __VLS_intrinsics.button)({
    ...{ onClick: (__VLS_ctx.loadFirmwareUrl) },
    ...{ class: "ghost" },
    disabled: (__VLS_ctx.busy || !__VLS_ctx.firmwareUrl),
});
/** @type {__VLS_StyleScopedClasses['ghost']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
    ...{ class: "divider" },
});
/** @type {__VLS_StyleScopedClasses['divider']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.span, __VLS_intrinsics.span)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.label, __VLS_intrinsics.label)({
    ...{ class: "file-picker" },
});
/** @type {__VLS_StyleScopedClasses['file-picker']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.input)({
    ...{ onChange: (__VLS_ctx.chooseFirmware) },
    type: "file",
    accept: ".bin,application/octet-stream",
});
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.label, __VLS_intrinsics.label)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.input)({
    placeholder: "必须与构建版本完全一致",
    maxlength: "31",
});
(__VLS_ctx.firmwareVersion);
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
    ...{ class: "firmware-meta" },
});
/** @type {__VLS_StyleScopedClasses['firmware-meta']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.strong, __VLS_intrinsics.strong)({});
(__VLS_ctx.firmwareName || '尚未选择固件');
__VLS_asFunctionalElement1(__VLS_intrinsics.span, __VLS_intrinsics.span)({});
(__VLS_ctx.firmware ? `${(__VLS_ctx.firmware.length / 1024).toFixed(1)} KiB` : '—');
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
    ...{ class: "progress" },
});
/** @type {__VLS_StyleScopedClasses['progress']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.div)({
    ...{ style: ({ width: `${__VLS_ctx.ota.percent}%` }) },
});
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
    ...{ class: "progress-meta" },
});
/** @type {__VLS_StyleScopedClasses['progress-meta']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.span, __VLS_intrinsics.span)({});
(__VLS_ctx.otaStateText);
__VLS_asFunctionalElement1(__VLS_intrinsics.span, __VLS_intrinsics.span)({});
(__VLS_ctx.ota.sent);
(__VLS_ctx.ota.total);
(__VLS_ctx.ota.percent);
__VLS_asFunctionalElement1(__VLS_intrinsics.button, __VLS_intrinsics.button)({
    ...{ onClick: (__VLS_ctx.startOta) },
    ...{ class: "danger" },
    disabled: (!__VLS_ctx.connected || !__VLS_ctx.firmware || !__VLS_ctx.firmwareVersion || __VLS_ctx.busy),
});
/** @type {__VLS_StyleScopedClasses['danger']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.p, __VLS_intrinsics.p)({
    ...{ class: "footnote" },
});
/** @type {__VLS_StyleScopedClasses['footnote']} */ ;
// @ts-ignore
[connected, busy, busy, firmwareUrl, firmwareUrl, loadFirmwareUrl, chooseFirmware, firmwareVersion, firmwareVersion, firmwareName, firmware, firmware, firmware, ota, ota, ota, ota, otaStateText, startOta,];
const __VLS_export = (await import('vue')).defineComponent({});
export default {};
