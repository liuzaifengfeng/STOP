<script setup lang="ts">
// 本页是双端通用说明，无需 BLE 连接；颜色示意不是设备实时状态。
defineOptions({ inheritAttrs: false })

const sections = [
  ['start', '快速上手'], ['indicators', '双端指示灯'], ['buzzer', '蜂鸣器'],
  ['estop', '急停与恢复'], ['protection', '保护规则'], ['tools', '网页与联调'],
] as const

const controllerLights = [
  { color: 'green', light: '绿色常亮', state: '已配对，急停触点正常', action: '到被控端确认心跳与输出状态。绿灯不表示对端已连接或已供电。' },
  { color: 'blue', light: '蓝色闪烁（亮 / 灭）', state: '未配对，急停触点正常', action: '让被控端开启配对窗口，等待双方保存配对。' },
  { color: 'red', light: '红色闪烁', state: '急停触点断开', action: '检查急停是否按下或触点线路断开；处理原因后释放急停。' },
  { color: 'orange', light: '橙红色常亮', state: '维护停机或低压停机过程', action: '等待维护结束，或为低电压设备充电；停机过程中不会允许供电。' },
  { color: 'cyan', light: '青色常亮', state: '10 秒反向诊断接收窗口', action: '安全心跳暂时停止，被控端保持断电；窗口结束后需重新本地确认供电。' },
  { color: 'red', light: '红色常亮', state: '当前无线链路初始化失败', action: '用网页或串口检查无线配置与模块。故障时仍可开放 BLE 诊断。' },
  { color: 'off', light: '熄灭', state: '可能已掉电、进入深睡眠或灯驱动故障', action: '低压睡眠后灯电源关闭；充电后重新上电或复位。仅凭灯灭不能判断对端输出。' },
]

const receiverLights = [
  { color: 'green', light: '绿色常亮', state: '输出已使能（ARMED）', action: '被控端已给出输出许可；实际输出电压仍需看设备状态。' },
  { color: 'blue', light: '蓝色闪烁（亮 / 灭）', state: '未配对（UNPAIRED）', action: '输出关闭。长按本地键 8 秒，开启配对窗口。' },
  { color: 'blue', light: '蓝色明暗交替（不全灭）', state: '30 秒配对窗口（PAIRING）', action: '输出关闭。等待控制端配对请求，成功后仍需本地确认使能。' },
  { color: 'orange', light: '橙红色常亮', state: '停机 / 等待确认（TRIPPED）', action: '输出关闭。包括上电、配对完成、远端 STOP、控制端重启等情况。' },
  { color: 'red', light: '红色闪烁', state: '运行中安全心跳失效（LINK_LOST）', action: '输出关闭。排查急停、距离、干扰、控制端电量或通信配置。' },
  { color: 'red', light: '红色常亮', state: '硬件故障（HARDWARE_FAULT）', action: '输出关闭。排查电压、电流、传感器和无线初始化；故障锁存不能用普通使能解除。' },
  { color: 'cyan', light: '青色常亮（按键期间）', state: '已识别按下，未满 3 秒', action: '继续按住；短按后松开不会恢复供电。' },
  { color: 'yellow', light: '黄色常亮（按键期间）', state: '已满 3 秒，未满 8 秒', action: '此时松开，才会尝试安全使能；黄灯不表示所有使能条件已满足。' },
  { color: 'purple', light: '紫色明暗交替（按键期间）', state: '已达到 8 秒配对时长', action: '输出断开时开启配对窗口；松开不会同时使能输出。' },
]

const protections = [
  ['控制端低电压', '电池 ≤ 3.5 V；约每秒更新电压', '上电时短鸣 200 ms，拒绝启动并深睡眠。运行中先锁定停机；已配对时请求对端失能，再短鸣并睡眠。电压回弹不会取消停机，充电后需重新上电 / 复位。'],
  ['停机回执', '每轮等 1.2 秒，最多 3 轮', '先撤销使能，再回执。“驱动关闭”与“驱动关闭且新采样 VOUT < 2 V”分开报告。控制端核对认证、设备和本次请求；无法确认时静默等 900 ms 后睡眠，不持续等待耗电。'],
  ['安全心跳失效', '800 ms 未收到有效安全心跳', '供电期间撤销输出许可并锁存停机。控制端断电、无线异常或无有效心跳都会影响供电；连接恢复后不会自动重启。'],
  ['输入电压越界', '使能前：9～30 V；运行中：低于 8.5 V 或高于 31 V', '使能前不符合范围会被拒绝；供电期间约每 500 ms 检查 VIN，越界或读取失败时锁存硬件故障并停机。'],
  ['软件过流', '电流 ≥ 5.5 A 持续约 200 ms', '撤销输出许可并锁存硬件故障。使能前电流也必须小于 5.5 A。'],
  ['大电流与硬件告警', '软件读数 ≥ 7 A；INA226 硬件门限 7 A', '软件检测到大电流立即撤销许可；硬件告警按实际电路作用。软件与硬件都需要实机验证，不把软件阈值当作整个电路的保证响应时间。'],
  ['输出异常', '使能满 500 ms 后 VOUT < 5 V；断开满 3 秒后 VOUT > 2 V', '分别视为无法建立输出、或断开后仍存在电压，锁存硬件故障。断开后的检查约每 500 ms 进行。停机回执采用独立的新采样检查。'],
  ['传感器 / 无线故障', '启动自检失败，或供电时 INA226 读取失败', '输出保持关闭或立即停机。故障排除后重新上电自检；本地长按及网页临时使能均不能绕过硬件故障锁存。'],
  ['OTA 升级', '进入维护 / 升级期间', '控制端进入维护停机；被控端供电时遇到 OTA 会撤销许可并锁存硬件故障。升级结束或设备重启后，不自动恢复输出。'],
]
</script>

<template>
  <div class="page-stack operating-guide">
    <section class="panel guide-intro">
      <span class="eyebrow">双端操作手册</span>
      <h2>先看状态，再确认供电</h2>
      <p>适用于当前项目固件；无需连接设备即可阅读。灯光示意只用于查表，实际状态以设备与输出测量为准。</p>
      <div class="guide-highlights">
        <div><strong>3.5 V</strong><span>控制端低压停机与睡眠</span></div>
        <div><strong>800 ms</strong><span>被控端有效心跳超时</span></div>
        <div><strong>3 秒 / 8 秒</strong><span>本地确认使能 / 开启配对</span></div>
      </div>
      <nav class="guide-index" aria-label="操作说明目录">
        <a v-for="[id, label] in sections" :key="id" :href="`#guide-${id}`">{{ label }}</a>
      </nav>
    </section>

    <section id="guide-start" class="panel guide-section">
      <h2>1. 快速上手：配对与供电</h2>
      <ol class="guide-steps">
        <li><b>上电并释放控制端急停。</b>控制端电池应高于 3.5 V；被控端上电默认输出关闭。两端无线模式及相关通信参数须匹配。</li>
        <li><b>在被控端长按 8 秒，开启 30 秒配对窗口。</b>只在输出关闭时配对。控制端自动发送配对请求；当前逻辑在未配对或急停触点断开时约每 5 秒请求一次。重新配对已有设备时可在输出断开后按下控制端急停，让它重新发出请求。</li>
        <li><b>等配对完成，再释放控制端急停。</b>配对结果保存在设备中，重启后保留。双方保存配对不等于供电，控制端绿灯也不等于被控端已使能。</li>
        <li><b>被控端本地键按住至少 3 秒、未满 8 秒时松开。</b>固件会检查连续 3 个安全心跳、心跳未超时、已配对、配对窗口关闭、无线可用、VIN 9～30 V、VOUT &lt; 2 V、电流 &lt; 5.5 A、INA226 告警可清除、无 OTA 和无硬件故障锁存；全部通过才使能输出。</li>
      </ol>
      <p class="guide-callout">被控端已经供电时，本地键不会执行配对，也不会作为断开按钮。需要停机时使用控制端急停，或网页的“断开输出”。</p>
    </section>

    <section id="guide-indicators" class="panel guide-section">
      <h2>2. 双端指示灯查表</h2>
      <p>闪烁通常每 250 ms 切换一次，完整周期约 500 ms。表中同时写出颜色和动作，不必只靠颜色判断。</p>
      <h3>控制端</h3>
      <div class="guide-table-wrap"><table>
        <caption class="guide-sr-only">控制端指示灯状态及操作</caption>
        <thead><tr><th scope="col">灯光</th><th scope="col">含义</th><th scope="col">下一步</th></tr></thead>
        <tbody><tr v-for="row in controllerLights" :key="row.light">
          <th scope="row"><span :class="['guide-led', `led-${row.color}`]" aria-hidden="true" />{{ row.light }}</th>
          <td data-label="含义">{{ row.state }}</td><td data-label="下一步">{{ row.action }}</td>
        </tr></tbody>
      </table></div>
      <p class="guide-note">多个状态重叠时，控制端显示优先级为：无线初始化故障 → 反向接收窗口 → 维护停机 → 急停 → 未配对 → 正常。</p>
      <h3>被控端</h3>
      <div class="guide-table-wrap"><table>
        <caption class="guide-sr-only">被控端指示灯状态及操作</caption>
        <thead><tr><th scope="col">灯光</th><th scope="col">含义</th><th scope="col">下一步</th></tr></thead>
        <tbody><tr v-for="row in receiverLights" :key="row.light">
          <th scope="row"><span :class="['guide-led', `led-${row.color}`]" aria-hidden="true" />{{ row.light }}</th>
          <td data-label="含义">{{ row.state }}</td><td data-label="下一步">{{ row.action }}</td>
        </tr></tbody>
      </table></div>
      <p class="guide-callout">被控端未使能时，按键的青 / 黄 / 紫色提示优先于状态颜色，即使存在硬件故障也可能显示黄灯。松开后再查看状态；黄色只是计时提示，不保证能恢复供电。</p>
    </section>

    <section id="guide-buzzer" class="panel guide-section">
      <h2>3. 蜂鸣器鸣叫规则</h2>
      <dl class="guide-facts">
        <div><dt>控制端正常启动</dt><dd>两声短鸣：每声约 100 ms，中间间隔约 100 ms。低压上电时不会进入正常启动流程。</dd></div>
        <div><dt>控制端运行提示</dt><dd>正常任务运行时约每 10 分钟短鸣一次，每次 200 ms。这是周期运行提示，不表示被控端已使能，也不是当前的低压报警。</dd></div>
        <div><dt>控制端低压停机</dt><dd>上电电压 ≤ 3.5 V：短鸣一次 200 ms 后睡眠。运行中达到阈值：先执行失能与回执流程，再短鸣一次 200 ms 后睡眠；睡眠后不持续鸣叫。</dd></div>
        <div><dt>急停、失联与硬件故障</dt><dd>当前没有专门的蜂鸣器鸣叫编码，按指示灯、网页状态和串口记录排查。不要等待蜂鸣器响才确认停机。</dd></div>
        <div><dt>被控端</dt><dd>当前固件没有被控端蜂鸣器控制逻辑，使用指示灯和状态读数提示。</dd></div>
      </dl>
    </section>

    <section id="guide-estop" class="panel guide-section">
      <h2>4. 急停与恢复规则</h2>
      <p>控制端采用常闭触点：正常闭合允许发送安全心跳，按下急停或触点线路断开时发送 STOP。急停沿额外发送 3 帧失能心跳；被控端收到有效 STOP 会撤销输出许可，运行中安全心跳无效或超时也会停机。</p>
      <ol class="guide-steps">
        <li><b>触发停机：</b>按下控制端急停。输出断开后保持停机，恢复通信或释放急停都不会自动供电。</li>
        <li><b>查明原因：</b>确认现场具备恢复条件，再处理急停、通信、电源或负载问题。红色常亮的硬件故障需排除原因并重新上电自检。</li>
        <li><b>本地重新确认：</b>释放控制端急停，等安全心跳恢复后，在被控端按住至少 3 秒、未满 8 秒时松开。固件实时复核所有使能条件，通过后才恢复输出。</li>
      </ol>
      <p class="guide-note">被控端重启、控制端重启、配对完成后均需重新确认。800 ms 是有效心跳超时门限，输出实际断开还包括任务调度与硬件动作时间。</p>
    </section>

    <section id="guide-protection" class="panel guide-section">
      <h2>5. 保护规则与停机回执</h2>
      <p>以下数值对应当前固件。网页的状态上报周期和诊断发送超时，不会改变安全心跳超时、电压或电流保护门限。</p>
      <div class="guide-table-wrap"><table class="guide-protection-table">
        <caption class="guide-sr-only">电池、电压、电流、通信和升级保护规则</caption>
        <thead><tr><th scope="col">保护项目</th><th scope="col">条件</th><th scope="col">动作与恢复</th></tr></thead>
        <tbody><tr v-for="[name, trigger, action] in protections" :key="name"><th scope="row">{{ name }}</th><td data-label="条件">{{ trigger }}</td><td data-label="动作与恢复">{{ action }}</td></tr></tbody>
      </table></div>
      <p class="guide-callout">失能回执需要双端都更新固件。“已撤销使能”不等于确认实际断电；VOUT &lt; 2 V 也是采样时刻的电气状态，不能替代独立的接触器触点反馈。没有回执时仍执行失联停机兜底。</p>
      <p class="guide-note">低压深睡眠没有配置自动唤醒。充电后重新上电或复位；若电压仍 ≤ 3.5 V，继续拒绝启动。低压等待回执期间不会重新发送使能心跳。</p>
    </section>

    <section id="guide-tools" class="panel guide-section">
      <h2>6. 网页、无线联调与常见问题</h2>
      <dl class="guide-facts">
        <div><dt>BLE 连接与安全无线连接</dt><dd>网页顶部“已连接”表示浏览器的 BLE 维护连接；供电依赖双端安全无线心跳。网页断开 BLE 不等于触发急停，双端配对成功也不保证此刻心跳有效。</dd></div>
        <div><dt>E22、ESP-NOW 与双通道</dt><dd>两端选择相容的无线模式；ESP-NOW 要匹配频道，E22 要匹配无线参数。设备配置写入后，按页面提示重启生效。双通道采用任一路有效报文，重复帧按序号去重；发送成功不等于对端已执行。</dd></div>
        <div><dt>配对窗口与临时调试</dt><dd>“配对与联调”可以在输出断开时开启 30 秒配对窗口。“临时调试：安全使能输出”会实际接通输出，但仍检查固件安全条件；只用于受控联调，不能绕过急停、失联或故障锁存。“断开输出”可主动撤销许可。</dd></div>
        <div><dt>反向无线诊断</dt><dd>双端连接网页且确认被控输出关闭后，页面可让控制端发送 STOP 并暂停心跳 10 秒，供被控端反向发包。窗口结束只恢复心跳，不自动恢复输出；需重新本地确认使能。</dd></div>
        <div><dt>黄灯后松开，却没有供电</dt><dd>黄灯只表示按键计时已满 3 秒。查看被控端心跳、配对目标、配对窗口、VIN / VOUT / 电流、OTA 与故障状态；具体拒绝原因见被控端串口。</dd></div>
        <div><dt>低压后网页掉线、灯灭</dt><dd>可能是控制端已深睡眠，BLE 和指示灯停止工作。先充电，再重新上电或复位；不要靠反复重启低压设备恢复使用。</dd></div>
        <div><dt>灯色与网页读数不一致</dt><dd>网页按上报周期更新，可能滞后；按键提示也可能覆盖状态灯。先松开按键、刷新状态，再查看串口与实际输出，不能用网页旧读数确认断电。</dd></div>
      </dl>
    </section>
  </div>
</template>

<style scoped>
.operating-guide { --guide-text: #b8c3cc; --guide-muted: #909fa9; --guide-border: #303943; --guide-tint: #ddb94f12; color: var(--guide-text); line-height: 1.8; font-size: 13px; }
:global(.theme-light .operating-guide) { --guide-text: #514540; --guide-muted: #73635c; --guide-border: #e4d9d2; --guide-tint: #a2503809; }
.guide-intro h2 { margin: 8px 0; font-size: clamp(23px, 3vw, 32px); color: inherit; }
.guide-section { scroll-margin-top: 95px; min-width: 0; }
.guide-section h2 { margin: 0 0 12px; font-size: 19px; color: inherit; }
.guide-section h3 { margin: 24px 0 10px; font-size: 15px; }
.guide-section p, .guide-intro p { margin: 10px 0; }
.guide-highlights { display: grid; grid-template-columns: repeat(3, minmax(0, 1fr)); margin: 22px 0; border: 1px solid var(--guide-border); border-radius: 8px; background: var(--guide-tint); }
.guide-highlights div { display: grid; gap: 3px; padding: 16px; }
.guide-highlights div + div { border-left: 1px solid var(--guide-border); }
.guide-highlights strong { font-size: 21px; }
.guide-highlights span { color: var(--guide-muted); font-size: 12px; }
.guide-index { display: flex; flex-wrap: wrap; gap: 8px; }
.guide-index a { padding: 7px 12px; border: 1px solid var(--guide-border); border-radius: 6px; text-decoration: none; color: inherit; }
.guide-index a:hover, .guide-index a:focus-visible { background: var(--guide-tint); outline: 2px solid #b69644; outline-offset: 2px; }
.guide-steps { padding-left: 24px; margin: 12px 0; }
.guide-steps li { padding: 6px 0 6px 5px; }
.guide-table-wrap { max-width: 100%; overflow-x: auto; border: 1px solid var(--guide-border); border-radius: 8px; }
table { width: 100%; min-width: 560px; border-collapse: collapse; font-size: 12px; text-align: left; }
th, td { padding: 13px 14px; vertical-align: top; border-bottom: 1px solid var(--guide-border); overflow-wrap: anywhere; }
thead { background: var(--guide-tint); }
tbody th { width: 24%; font-weight: 600; }
tbody td:nth-child(2) { width: 27%; }
tbody tr:last-child th, tbody tr:last-child td { border-bottom: 0; }
.guide-led { display: inline-block; width: 10px; height: 10px; margin-right: 8px; border: 1px solid #0003; border-radius: 50%; }
.led-green { background: #30b577; }.led-blue { background: #5189ed; }.led-red { background: #ed5757; }.led-orange { background: #ef8851; }.led-cyan { background: #36bbc9; }.led-yellow { background: #d4b122; }.led-purple { background: #b271e9; }.led-off { background: #7d858e; }
.guide-protection-table tbody th { width: 19%; }.guide-protection-table tbody td:nth-child(2) { width: 28%; }
.guide-callout { padding: 12px 14px; border-left: 3px solid #b69644; border-radius: 0 6px 6px 0; background: var(--guide-tint); }
.guide-note { color: var(--guide-muted); font-size: 12px; }
.guide-facts { margin: 0; }
.guide-facts div { padding: 13px 0; border-bottom: 1px solid var(--guide-border); }
.guide-facts div:last-child { border: 0; }
.guide-facts dt { font-weight: 600; margin-bottom: 4px; }
.guide-facts dd { margin: 0; }
.guide-sr-only { position: absolute; width: 1px; height: 1px; padding: 0; overflow: hidden; clip: rect(0, 0, 0, 0); white-space: nowrap; }
@media (max-width: 650px) { .operating-guide .panel { padding: 17px; }.guide-highlights { grid-template-columns: 1fr; }.guide-highlights div { padding: 10px 14px; }.guide-highlights div + div { border-left: 0; border-top: 1px solid var(--guide-border); }.guide-highlights strong { font-size: 19px; }.guide-index a { padding: 6px 9px; }.guide-section h2 { font-size: 17px; } }
@media (max-width: 650px) {
  table { display: block; min-width: 0; font-size: 13px; }
  thead { position: absolute; width: 1px; height: 1px; overflow: hidden; clip: rect(0, 0, 0, 0); }
  tbody { display: block; }
  tbody tr { display: grid; padding: 12px 14px; border-bottom: 1px solid var(--guide-border); }
  tbody tr:last-child { border-bottom: 0; }
  tbody th, tbody td, .guide-protection-table tbody th, .guide-protection-table tbody td:nth-child(2), tbody td:nth-child(2) { display: block; width: auto; padding: 4px 0; border: 0; }
  tbody td::before { content: attr(data-label) '：'; font-weight: 600; }
}
</style>
/// <reference types="C:/Users/LZF/Documents/PlatformIO/Projects/STOP_wireless/web/node_modules/@vue/language-core/types/template-helpers.d.ts" />
/// <reference types="C:/Users/LZF/Documents/PlatformIO/Projects/STOP_wireless/web/node_modules/@vue/language-core/types/props-fallback.d.ts" />

// 本页是双端通用说明，无需 BLE 连接；颜色示意不是设备实时状态。
defineOptions({ inheritAttrs: false });
const sections = [
    ['start', '快速上手'], ['indicators', '双端指示灯'], ['buzzer', '蜂鸣器'],
    ['estop', '急停与恢复'], ['protection', '保护规则'], ['tools', '网页与联调'],
];
const controllerLights = [
    { color: 'green', light: '绿色常亮', state: '已配对，急停触点正常', action: '到被控端确认心跳与输出状态。绿灯不表示对端已连接或已供电。' },
    { color: 'blue', light: '蓝色闪烁（亮 / 灭）', state: '未配对，急停触点正常', action: '让被控端开启配对窗口，等待双方保存配对。' },
    { color: 'red', light: '红色闪烁', state: '急停触点断开', action: '检查急停是否按下或触点线路断开；处理原因后释放急停。' },
    { color: 'orange', light: '橙红色常亮', state: '维护停机或低压停机过程', action: '等待维护结束，或为低电压设备充电；停机过程中不会允许供电。' },
    { color: 'cyan', light: '青色常亮', state: '10 秒反向诊断接收窗口', action: '安全心跳暂时停止，被控端保持断电；窗口结束后需重新本地确认供电。' },
    { color: 'red', light: '红色常亮', state: '当前无线链路初始化失败', action: '用网页或串口检查无线配置与模块。故障时仍可开放 BLE 诊断。' },
    { color: 'off', light: '熄灭', state: '可能已掉电、进入深睡眠或灯驱动故障', action: '低压睡眠后灯电源关闭；充电后重新上电或复位。仅凭灯灭不能判断对端输出。' },
];
const receiverLights = [
    { color: 'green', light: '绿色常亮', state: '输出已使能（ARMED）', action: '被控端已给出输出许可；实际输出电压仍需看设备状态。' },
    { color: 'blue', light: '蓝色闪烁（亮 / 灭）', state: '未配对（UNPAIRED）', action: '输出关闭。长按本地键 8 秒，开启配对窗口。' },
    { color: 'blue', light: '蓝色明暗交替（不全灭）', state: '30 秒配对窗口（PAIRING）', action: '输出关闭。等待控制端配对请求，成功后仍需本地确认使能。' },
    { color: 'orange', light: '橙红色常亮', state: '停机 / 等待确认（TRIPPED）', action: '输出关闭。包括上电、配对完成、远端 STOP、控制端重启等情况。' },
    { color: 'red', light: '红色闪烁', state: '运行中安全心跳失效（LINK_LOST）', action: '输出关闭。排查急停、距离、干扰、控制端电量或通信配置。' },
    { color: 'red', light: '红色常亮', state: '硬件故障（HARDWARE_FAULT）', action: '输出关闭。排查电压、电流、传感器和无线初始化；故障锁存不能用普通使能解除。' },
    { color: 'cyan', light: '青色常亮（按键期间）', state: '已识别按下，未满 3 秒', action: '继续按住；短按后松开不会恢复供电。' },
    { color: 'yellow', light: '黄色常亮（按键期间）', state: '已满 3 秒，未满 8 秒', action: '此时松开，才会尝试安全使能；黄灯不表示所有使能条件已满足。' },
    { color: 'purple', light: '紫色明暗交替（按键期间）', state: '已达到 8 秒配对时长', action: '输出断开时开启配对窗口；松开不会同时使能输出。' },
];
const protections = [
    ['控制端低电压', '电池 ≤ 3.5 V；约每秒更新电压', '上电时短鸣 200 ms，拒绝启动并深睡眠。运行中先锁定停机；已配对时请求对端失能，再短鸣并睡眠。电压回弹不会取消停机，充电后需重新上电 / 复位。'],
    ['停机回执', '每轮等 1.2 秒，最多 3 轮', '先撤销使能，再回执。“驱动关闭”与“驱动关闭且新采样 VOUT < 2 V”分开报告。控制端核对认证、设备和本次请求；无法确认时静默等 900 ms 后睡眠，不持续等待耗电。'],
    ['安全心跳失效', '800 ms 未收到有效安全心跳', '供电期间撤销输出许可并锁存停机。控制端断电、无线异常或无有效心跳都会影响供电；连接恢复后不会自动重启。'],
    ['输入电压越界', '使能前：9～30 V；运行中：低于 8.5 V 或高于 31 V', '使能前不符合范围会被拒绝；供电期间约每 500 ms 检查 VIN，越界或读取失败时锁存硬件故障并停机。'],
    ['软件过流', '电流 ≥ 5.5 A 持续约 200 ms', '撤销输出许可并锁存硬件故障。使能前电流也必须小于 5.5 A。'],
    ['大电流与硬件告警', '软件读数 ≥ 7 A；INA226 硬件门限 7 A', '软件检测到大电流立即撤销许可；硬件告警按实际电路作用。软件与硬件都需要实机验证，不把软件阈值当作整个电路的保证响应时间。'],
    ['输出异常', '使能满 500 ms 后 VOUT < 5 V；断开满 3 秒后 VOUT > 2 V', '分别视为无法建立输出、或断开后仍存在电压，锁存硬件故障。断开后的检查约每 500 ms 进行。停机回执采用独立的新采样检查。'],
    ['传感器 / 无线故障', '启动自检失败，或供电时 INA226 读取失败', '输出保持关闭或立即停机。故障排除后重新上电自检；本地长按及网页临时使能均不能绕过硬件故障锁存。'],
    ['OTA 升级', '进入维护 / 升级期间', '控制端进入维护停机；被控端供电时遇到 OTA 会撤销许可并锁存硬件故障。升级结束或设备重启后，不自动恢复输出。'],
];
const __VLS_ctx = {
    ...{},
    ...{},
};
let __VLS_components;
let __VLS_intrinsics;
let __VLS_directives;
/** @type {__VLS_StyleScopedClasses['operating-guide']} */ ;
/** @type {__VLS_StyleScopedClasses['guide-section']} */ ;
/** @type {__VLS_StyleScopedClasses['guide-section']} */ ;
/** @type {__VLS_StyleScopedClasses['guide-section']} */ ;
/** @type {__VLS_StyleScopedClasses['guide-intro']} */ ;
/** @type {__VLS_StyleScopedClasses['guide-highlights']} */ ;
/** @type {__VLS_StyleScopedClasses['guide-highlights']} */ ;
/** @type {__VLS_StyleScopedClasses['guide-highlights']} */ ;
/** @type {__VLS_StyleScopedClasses['guide-highlights']} */ ;
/** @type {__VLS_StyleScopedClasses['guide-index']} */ ;
/** @type {__VLS_StyleScopedClasses['guide-index']} */ ;
/** @type {__VLS_StyleScopedClasses['guide-index']} */ ;
/** @type {__VLS_StyleScopedClasses['guide-steps']} */ ;
/** @type {__VLS_StyleScopedClasses['guide-protection-table']} */ ;
/** @type {__VLS_StyleScopedClasses['guide-facts']} */ ;
/** @type {__VLS_StyleScopedClasses['guide-facts']} */ ;
/** @type {__VLS_StyleScopedClasses['guide-facts']} */ ;
/** @type {__VLS_StyleScopedClasses['guide-facts']} */ ;
/** @type {__VLS_StyleScopedClasses['operating-guide']} */ ;
/** @type {__VLS_StyleScopedClasses['guide-highlights']} */ ;
/** @type {__VLS_StyleScopedClasses['guide-highlights']} */ ;
/** @type {__VLS_StyleScopedClasses['guide-highlights']} */ ;
/** @type {__VLS_StyleScopedClasses['guide-highlights']} */ ;
/** @type {__VLS_StyleScopedClasses['guide-index']} */ ;
/** @type {__VLS_StyleScopedClasses['guide-section']} */ ;
/** @type {__VLS_StyleScopedClasses['guide-protection-table']} */ ;
/** @type {__VLS_StyleScopedClasses['guide-protection-table']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
    ...{ class: "page-stack operating-guide" },
});
/** @type {__VLS_StyleScopedClasses['page-stack']} */ ;
/** @type {__VLS_StyleScopedClasses['operating-guide']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.section, __VLS_intrinsics.section)({
    ...{ class: "panel guide-intro" },
});
/** @type {__VLS_StyleScopedClasses['panel']} */ ;
/** @type {__VLS_StyleScopedClasses['guide-intro']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.span, __VLS_intrinsics.span)({
    ...{ class: "eyebrow" },
});
/** @type {__VLS_StyleScopedClasses['eyebrow']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.h2, __VLS_intrinsics.h2)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.p, __VLS_intrinsics.p)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
    ...{ class: "guide-highlights" },
});
/** @type {__VLS_StyleScopedClasses['guide-highlights']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.strong, __VLS_intrinsics.strong)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.span, __VLS_intrinsics.span)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.strong, __VLS_intrinsics.strong)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.span, __VLS_intrinsics.span)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.strong, __VLS_intrinsics.strong)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.span, __VLS_intrinsics.span)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.nav, __VLS_intrinsics.nav)({
    ...{ class: "guide-index" },
    'aria-label': "操作说明目录",
});
/** @type {__VLS_StyleScopedClasses['guide-index']} */ ;
for (const [[id, label]] of __VLS_vFor((__VLS_ctx.sections))) {
    __VLS_asFunctionalElement1(__VLS_intrinsics.a, __VLS_intrinsics.a)({
        key: (id),
        href: (`#guide-${id}`),
    });
    (label);
    // @ts-ignore
    [sections,];
}
__VLS_asFunctionalElement1(__VLS_intrinsics.section, __VLS_intrinsics.section)({
    id: "guide-start",
    ...{ class: "panel guide-section" },
});
/** @type {__VLS_StyleScopedClasses['panel']} */ ;
/** @type {__VLS_StyleScopedClasses['guide-section']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.h2, __VLS_intrinsics.h2)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.ol, __VLS_intrinsics.ol)({
    ...{ class: "guide-steps" },
});
/** @type {__VLS_StyleScopedClasses['guide-steps']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.li, __VLS_intrinsics.li)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.b, __VLS_intrinsics.b)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.li, __VLS_intrinsics.li)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.b, __VLS_intrinsics.b)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.li, __VLS_intrinsics.li)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.b, __VLS_intrinsics.b)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.li, __VLS_intrinsics.li)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.b, __VLS_intrinsics.b)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.p, __VLS_intrinsics.p)({
    ...{ class: "guide-callout" },
});
/** @type {__VLS_StyleScopedClasses['guide-callout']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.section, __VLS_intrinsics.section)({
    id: "guide-indicators",
    ...{ class: "panel guide-section" },
});
/** @type {__VLS_StyleScopedClasses['panel']} */ ;
/** @type {__VLS_StyleScopedClasses['guide-section']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.h2, __VLS_intrinsics.h2)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.p, __VLS_intrinsics.p)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.h3, __VLS_intrinsics.h3)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
    ...{ class: "guide-table-wrap" },
});
/** @type {__VLS_StyleScopedClasses['guide-table-wrap']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.table, __VLS_intrinsics.table)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.caption, __VLS_intrinsics.caption)({
    ...{ class: "guide-sr-only" },
});
/** @type {__VLS_StyleScopedClasses['guide-sr-only']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.thead, __VLS_intrinsics.thead)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.tr, __VLS_intrinsics.tr)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.th, __VLS_intrinsics.th)({
    scope: "col",
});
__VLS_asFunctionalElement1(__VLS_intrinsics.th, __VLS_intrinsics.th)({
    scope: "col",
});
__VLS_asFunctionalElement1(__VLS_intrinsics.th, __VLS_intrinsics.th)({
    scope: "col",
});
__VLS_asFunctionalElement1(__VLS_intrinsics.tbody, __VLS_intrinsics.tbody)({});
for (const [row] of __VLS_vFor((__VLS_ctx.controllerLights))) {
    __VLS_asFunctionalElement1(__VLS_intrinsics.tr, __VLS_intrinsics.tr)({
        key: (row.light),
    });
    __VLS_asFunctionalElement1(__VLS_intrinsics.th, __VLS_intrinsics.th)({
        scope: "row",
    });
    __VLS_asFunctionalElement1(__VLS_intrinsics.span)({
        ...{ class: (['guide-led', `led-${row.color}`]) },
        'aria-hidden': "true",
    });
    /** @type {__VLS_StyleScopedClasses['guide-led']} */ ;
    (row.light);
    __VLS_asFunctionalElement1(__VLS_intrinsics.td, __VLS_intrinsics.td)({
        'data-label': "含义",
    });
    (row.state);
    __VLS_asFunctionalElement1(__VLS_intrinsics.td, __VLS_intrinsics.td)({
        'data-label': "下一步",
    });
    (row.action);
    // @ts-ignore
    [controllerLights,];
}
__VLS_asFunctionalElement1(__VLS_intrinsics.p, __VLS_intrinsics.p)({
    ...{ class: "guide-note" },
});
/** @type {__VLS_StyleScopedClasses['guide-note']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.h3, __VLS_intrinsics.h3)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
    ...{ class: "guide-table-wrap" },
});
/** @type {__VLS_StyleScopedClasses['guide-table-wrap']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.table, __VLS_intrinsics.table)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.caption, __VLS_intrinsics.caption)({
    ...{ class: "guide-sr-only" },
});
/** @type {__VLS_StyleScopedClasses['guide-sr-only']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.thead, __VLS_intrinsics.thead)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.tr, __VLS_intrinsics.tr)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.th, __VLS_intrinsics.th)({
    scope: "col",
});
__VLS_asFunctionalElement1(__VLS_intrinsics.th, __VLS_intrinsics.th)({
    scope: "col",
});
__VLS_asFunctionalElement1(__VLS_intrinsics.th, __VLS_intrinsics.th)({
    scope: "col",
});
__VLS_asFunctionalElement1(__VLS_intrinsics.tbody, __VLS_intrinsics.tbody)({});
for (const [row] of __VLS_vFor((__VLS_ctx.receiverLights))) {
    __VLS_asFunctionalElement1(__VLS_intrinsics.tr, __VLS_intrinsics.tr)({
        key: (row.light),
    });
    __VLS_asFunctionalElement1(__VLS_intrinsics.th, __VLS_intrinsics.th)({
        scope: "row",
    });
    __VLS_asFunctionalElement1(__VLS_intrinsics.span)({
        ...{ class: (['guide-led', `led-${row.color}`]) },
        'aria-hidden': "true",
    });
    /** @type {__VLS_StyleScopedClasses['guide-led']} */ ;
    (row.light);
    __VLS_asFunctionalElement1(__VLS_intrinsics.td, __VLS_intrinsics.td)({
        'data-label': "含义",
    });
    (row.state);
    __VLS_asFunctionalElement1(__VLS_intrinsics.td, __VLS_intrinsics.td)({
        'data-label': "下一步",
    });
    (row.action);
    // @ts-ignore
    [receiverLights,];
}
__VLS_asFunctionalElement1(__VLS_intrinsics.p, __VLS_intrinsics.p)({
    ...{ class: "guide-callout" },
});
/** @type {__VLS_StyleScopedClasses['guide-callout']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.section, __VLS_intrinsics.section)({
    id: "guide-buzzer",
    ...{ class: "panel guide-section" },
});
/** @type {__VLS_StyleScopedClasses['panel']} */ ;
/** @type {__VLS_StyleScopedClasses['guide-section']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.h2, __VLS_intrinsics.h2)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.dl, __VLS_intrinsics.dl)({
    ...{ class: "guide-facts" },
});
/** @type {__VLS_StyleScopedClasses['guide-facts']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.dt, __VLS_intrinsics.dt)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.dd, __VLS_intrinsics.dd)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.dt, __VLS_intrinsics.dt)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.dd, __VLS_intrinsics.dd)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.dt, __VLS_intrinsics.dt)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.dd, __VLS_intrinsics.dd)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.dt, __VLS_intrinsics.dt)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.dd, __VLS_intrinsics.dd)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.dt, __VLS_intrinsics.dt)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.dd, __VLS_intrinsics.dd)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.section, __VLS_intrinsics.section)({
    id: "guide-estop",
    ...{ class: "panel guide-section" },
});
/** @type {__VLS_StyleScopedClasses['panel']} */ ;
/** @type {__VLS_StyleScopedClasses['guide-section']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.h2, __VLS_intrinsics.h2)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.p, __VLS_intrinsics.p)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.ol, __VLS_intrinsics.ol)({
    ...{ class: "guide-steps" },
});
/** @type {__VLS_StyleScopedClasses['guide-steps']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.li, __VLS_intrinsics.li)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.b, __VLS_intrinsics.b)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.li, __VLS_intrinsics.li)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.b, __VLS_intrinsics.b)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.li, __VLS_intrinsics.li)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.b, __VLS_intrinsics.b)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.p, __VLS_intrinsics.p)({
    ...{ class: "guide-note" },
});
/** @type {__VLS_StyleScopedClasses['guide-note']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.section, __VLS_intrinsics.section)({
    id: "guide-protection",
    ...{ class: "panel guide-section" },
});
/** @type {__VLS_StyleScopedClasses['panel']} */ ;
/** @type {__VLS_StyleScopedClasses['guide-section']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.h2, __VLS_intrinsics.h2)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.p, __VLS_intrinsics.p)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({
    ...{ class: "guide-table-wrap" },
});
/** @type {__VLS_StyleScopedClasses['guide-table-wrap']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.table, __VLS_intrinsics.table)({
    ...{ class: "guide-protection-table" },
});
/** @type {__VLS_StyleScopedClasses['guide-protection-table']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.caption, __VLS_intrinsics.caption)({
    ...{ class: "guide-sr-only" },
});
/** @type {__VLS_StyleScopedClasses['guide-sr-only']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.thead, __VLS_intrinsics.thead)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.tr, __VLS_intrinsics.tr)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.th, __VLS_intrinsics.th)({
    scope: "col",
});
__VLS_asFunctionalElement1(__VLS_intrinsics.th, __VLS_intrinsics.th)({
    scope: "col",
});
__VLS_asFunctionalElement1(__VLS_intrinsics.th, __VLS_intrinsics.th)({
    scope: "col",
});
__VLS_asFunctionalElement1(__VLS_intrinsics.tbody, __VLS_intrinsics.tbody)({});
for (const [[name, trigger, action]] of __VLS_vFor((__VLS_ctx.protections))) {
    __VLS_asFunctionalElement1(__VLS_intrinsics.tr, __VLS_intrinsics.tr)({
        key: (name),
    });
    __VLS_asFunctionalElement1(__VLS_intrinsics.th, __VLS_intrinsics.th)({
        scope: "row",
    });
    (name);
    __VLS_asFunctionalElement1(__VLS_intrinsics.td, __VLS_intrinsics.td)({
        'data-label': "条件",
    });
    (trigger);
    __VLS_asFunctionalElement1(__VLS_intrinsics.td, __VLS_intrinsics.td)({
        'data-label': "动作与恢复",
    });
    (action);
    // @ts-ignore
    [protections,];
}
__VLS_asFunctionalElement1(__VLS_intrinsics.p, __VLS_intrinsics.p)({
    ...{ class: "guide-callout" },
});
/** @type {__VLS_StyleScopedClasses['guide-callout']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.p, __VLS_intrinsics.p)({
    ...{ class: "guide-note" },
});
/** @type {__VLS_StyleScopedClasses['guide-note']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.section, __VLS_intrinsics.section)({
    id: "guide-tools",
    ...{ class: "panel guide-section" },
});
/** @type {__VLS_StyleScopedClasses['panel']} */ ;
/** @type {__VLS_StyleScopedClasses['guide-section']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.h2, __VLS_intrinsics.h2)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.dl, __VLS_intrinsics.dl)({
    ...{ class: "guide-facts" },
});
/** @type {__VLS_StyleScopedClasses['guide-facts']} */ ;
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.dt, __VLS_intrinsics.dt)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.dd, __VLS_intrinsics.dd)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.dt, __VLS_intrinsics.dt)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.dd, __VLS_intrinsics.dd)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.dt, __VLS_intrinsics.dt)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.dd, __VLS_intrinsics.dd)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.dt, __VLS_intrinsics.dt)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.dd, __VLS_intrinsics.dd)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.dt, __VLS_intrinsics.dt)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.dd, __VLS_intrinsics.dd)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.dt, __VLS_intrinsics.dt)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.dd, __VLS_intrinsics.dd)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.div, __VLS_intrinsics.div)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.dt, __VLS_intrinsics.dt)({});
__VLS_asFunctionalElement1(__VLS_intrinsics.dd, __VLS_intrinsics.dd)({});
// @ts-ignore
[];
const __VLS_export = (await import('vue')).defineComponent({});
export default {};
