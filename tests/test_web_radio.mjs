// 在 Node 中执行真实 TypeScript 客户端，替换 BLE 请求以验证新旧配置兼容。
import assert from 'node:assert/strict'
import { readFileSync } from 'node:fs'
import ts from '../web/node_modules/typescript/lib/typescript.js'

const transpile = (name) => ts.transpileModule(readFileSync(new URL(`../web/src/${name}.ts`, import.meta.url), 'utf8'), {
  compilerOptions: { target: ts.ScriptTarget.ES2022, module: ts.ModuleKind.ES2022 },
}).outputText
const uri = (source) => `data:text/javascript;base64,${Buffer.from(source).toString('base64')}`
const source = transpile('device-client').replace(/from ['"]\.\/protocol['"]/, `from '${uri(transpile('protocol'))}'`)
const { StopDeviceClient } = await import(uri(source))
const client = new StopDeviceClient('TEST')
let lastPayload
client.request = async (_type, payload) => {
  if (payload?.length > 1) lastPayload = payload
  return { payload: lastPayload }
}

const legacy = { alias: '测试设备', statusPeriodMs: 5000, radioTxTimeoutMs: 1000 }
await client.setConfig(legacy)
assert.equal(lastPayload[0], 3)
assert.deepEqual(await client.getConfig(), { ...legacy, radioLink: undefined, espnowChannel: undefined })
for (const radioLink of [0, 1, 2]) {
  const expected = { ...legacy, radioLink, espnowChannel: 11 }
  assert.deepEqual(await client.setConfig(expected), expected)
  assert.equal(lastPayload[0], 5)
  const tail = new DataView(lastPayload.buffer, lastPayload.length - 18)
  assert.equal(tail.getUint16(0, true), 4)
  assert.equal(tail.getUint32(5, true), radioLink)
  assert.equal(tail.getUint16(9, true), 5)
  assert.equal(tail.getUint32(14, true), 11)
  assert.deepEqual(await client.getConfig(), expected)
}
for (const [radioLink, espnowChannel] of [[3, 6], [-1, 6], [1, 0], [1, 12], [1, 1.5]]) {
  await assert.rejects(client.setConfig({ ...legacy, radioLink, espnowChannel }), /无线模式/)
}
lastPayload = new Uint8Array([...lastPayload, 0])
await assert.rejects(client.getConfig(), /多余数据/)
console.log('PASS: 新旧固件配置兼容、TLV 字段映射、模式/信道范围、回读与损坏包检查')
