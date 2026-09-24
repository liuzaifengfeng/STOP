import { computed, ref, watch } from 'vue'

export type ThemeMode = 'system' | 'light' | 'dark'
export type SidebarMode = 'expanded' | 'auto' | 'collapsed'
export type Language = 'zh-CN' | 'en'

const THEME_KEY = 'stop-c6.theme'
const SIDEBAR_KEY = 'stop-c6.sidebar'
const LANGUAGE_KEY = 'stop-c6.language'

const theme = ref<ThemeMode>(readChoice(THEME_KEY, ['system', 'light', 'dark'], 'system'))
const sidebar = ref<SidebarMode>(readChoice(SIDEBAR_KEY, ['expanded', 'auto', 'collapsed'], 'auto'))
const language = ref<Language>(readChoice(LANGUAGE_KEY, ['zh-CN', 'en'], 'zh-CN'))
const systemDark = ref(window.matchMedia?.('(prefers-color-scheme: dark)').matches ?? true)

window.matchMedia?.('(prefers-color-scheme: dark)').addEventListener('change', (event) => { systemDark.value = event.matches })
watch(theme, (value) => localStorage.setItem(THEME_KEY, value))
watch(sidebar, (value) => localStorage.setItem(SIDEBAR_KEY, value))
watch(language, (value) => localStorage.setItem(LANGUAGE_KEY, value))

const messages = {
  'zh-CN': {
    dashboard: '设备概览', dashboardCaption: '状态与连接', configuration: '设备配置', configurationCaption: '基础参数',
    radio: '无线诊断', radioCaption: 'E22 收发', firmware: '固件升级', firmwareCaption: 'BLE OTA', settings: '设置', settingsCaption: '外观与语言',
    noDevice: '没有设备', waitingConnection: '等待连接', bleConnected: 'BLE 已连接', maintenancePlane: '维护管理平面', safetyLock: '不得用于解除安全锁存',
    connect: '连接设备', disconnect: '断开', quality: '质量', safetyBoundary: '安全边界', safetyCopy: '此工具仅用于维护、诊断与升级，不能解除急停、复位安全锁存或强制恢复供电。',
    connectedDevice: '已连接设备', waitingIdentity: '等待设备标识', refresh: '刷新状态', safetyStatus: '安全状态', unavailable: '未接入', safetyMissing: 'safety_manager 尚未实现', battery: '电池电量', waitingStatus: '等待设备状态', uptime: '运行时间', uptimeHint: '设备连续运行时间', currentValue: '当前协商值', deviceInfo: '设备详情', firmwareVersion: '固件版本', productId: '产品编号', hardwareRevision: '硬件修订', protocolVersion: '协议版本', radioModule: '无线模块', available: '可用', notAvailable: '不可用', otaStatus: 'OTA 状态',
    basicParameters: '基础参数', storedNvs: '配置保存在设备非易失存储中，重启后继续生效。', alias: '设备别名', statusPeriod: '状态上报周期（ms）', radioTimeout: 'E22 发送超时（ms）', saveDevice: '验证并写入设备',
    radioSend: 'E22 诊断发送', text: '文本', diagnosticWarning: '诊断通道不应承载任何安全控制报文。', inputDiagnostic: '输入诊断数据', sendPacket: '发送诊断包', traffic: '收发记录', frame: '帧', noData: '暂无数据', noDataHint: '连接设备并发送诊断包后，记录会显示在这里。',
    bleUpgrade: 'BLE 固件升级', devOta: '开发级 OTA', cloudUrl: '云端固件 URL', download: '下载', or: '或者', chooseLocal: '选择本地固件', firmwareSupport: '支持 ESP-IDF app firmware.bin', embeddedVersion: '固件内嵌版本', versionPlaceholder: '必须与构建版本一致', selectedFirmware: '已选固件', noFile: '尚未选择文件', startUpgrade: '校验并开始升级', otaWarning: '升级期间请保持页面前台、电源稳定和设备在通信范围内。当前构建尚未启用固件签名，不能用于量产安全升级。',
    appearance: '界面偏好', sidebarBehavior: '侧边栏扩展', expanded: '展开', automatic: '自动', collapsed: '缩回', theme: '主题', system: '系统', light: '浅色', dark: '暗色', accent: '颜色主题', accentDefault: 'Chameleon 金', switchLanguage: '切换语言', languageName: '中文', savedAutomatically: '设置会自动保存在此浏览器中。',
  },
  en: {
    dashboard: 'Dashboard', dashboardCaption: 'Status & connection', configuration: 'Device config', configurationCaption: 'Parameters',
    radio: 'Radio diagnostics', radioCaption: 'E22 terminal', firmware: 'Firmware', firmwareCaption: 'BLE OTA', settings: 'Settings', settingsCaption: 'Appearance & language',
    noDevice: 'No device', waitingConnection: 'Waiting to connect', bleConnected: 'BLE connected', maintenancePlane: 'Maintenance plane', safetyLock: 'Cannot release safety latches',
    connect: 'Connect device', disconnect: 'Disconnect', quality: 'Quality', safetyBoundary: 'Safety boundary', safetyCopy: 'This tool is for maintenance, diagnostics and updates only. It cannot release E-stop or safety latches, or restore power.',
    connectedDevice: 'CONNECTED DEVICE', waitingIdentity: 'Waiting for device identity', refresh: 'Refresh', safetyStatus: 'Safety status', unavailable: 'Unavailable', safetyMissing: 'safety_manager not implemented', battery: 'Battery', waitingStatus: 'Waiting for status', uptime: 'Uptime', uptimeHint: 'Continuous device uptime', currentValue: 'Negotiated value', deviceInfo: 'Device details', firmwareVersion: 'Firmware', productId: 'Product ID', hardwareRevision: 'Hardware revision', protocolVersion: 'Protocol version', radioModule: 'Radio module', available: 'Available', notAvailable: 'Unavailable', otaStatus: 'OTA status',
    basicParameters: 'Basic parameters', storedNvs: 'Settings are stored in device NVS and persist after reboot.', alias: 'Device alias', statusPeriod: 'Status interval (ms)', radioTimeout: 'E22 timeout (ms)', saveDevice: 'Validate and save',
    radioSend: 'E22 diagnostic send', text: 'Text', diagnosticWarning: 'The diagnostic channel must not carry safety-control messages.', inputDiagnostic: 'Enter diagnostic data', sendPacket: 'Send packet', traffic: 'Traffic log', frame: 'frames', noData: 'No data', noDataHint: 'Connect a device and send a packet to see traffic here.',
    bleUpgrade: 'BLE firmware update', devOta: 'Development OTA', cloudUrl: 'Firmware URL', download: 'Download', or: 'or', chooseLocal: 'Choose local firmware', firmwareSupport: 'Supports ESP-IDF app firmware.bin', embeddedVersion: 'Embedded version', versionPlaceholder: 'Must match the build version', selectedFirmware: 'Selected firmware', noFile: 'No file selected', startUpgrade: 'Verify and start update', otaWarning: 'Keep this page in the foreground and maintain stable power and radio range. Firmware signing is not enabled; do not use this build for production updates.',
    appearance: 'Preferences', sidebarBehavior: 'Sidebar expansion', expanded: 'Expanded', automatic: 'Auto', collapsed: 'Collapsed', theme: 'Theme', system: 'System', light: 'Light', dark: 'Dark', accent: 'Color theme', accentDefault: 'Chameleon Gold', switchLanguage: 'Language', languageName: 'English', savedAutomatically: 'Settings are saved automatically in this browser.',
  },
} as const

type MessageKey = keyof typeof messages['zh-CN']

function readChoice<T extends string>(key: string, choices: readonly T[], fallback: T): T {
  const value = localStorage.getItem(key) as T | null
  return value && choices.includes(value) ? value : fallback
}

export function usePreferences() {
  const resolvedTheme = computed<'light' | 'dark'>(() => theme.value === 'system' ? (systemDark.value ? 'dark' : 'light') : theme.value)
  const t = (key: MessageKey): string => messages[language.value][key]
  return { theme, sidebar, language, resolvedTheme, t }
}
