#!/usr/bin/env node
/**
 * 智联迷城 · MQTT Broker（aedes）
 *
 * 为什么不用 mosquitto：mosquitto 的密码是静态文件，服务器必须事先知道每组的密码；
 * 而密码 = f(组号, 板子 MAC)，老师收不齐 MAC 就永远建不全表。
 *
 * 这里改成「首次连接自动登记」（TOFU, trust on first use）：
 *   1. 组号首次出现 —— 接受它上报的密码，记进 accounts.json；
 *   2. 之后再用该组号 —— 密码必须与记录一致，否则拒绝；
 *   3. 若该组尚未开始答题（分数 0 且关卡 0），允许用新密码覆盖，避免第一次手滑锁死；
 *   4. 每次判定都写日志，老师能看到谁在被拒。
 *
 * 学生侧零改动：固件仍按规范用「组号 + MAC」算密码，第一次连上就算完成授权。
 *
 * 回滚：systemctl stop iot-broker && systemctl start mosquitto
 */
'use strict';
const fs = require('fs');
const path = require('path');
const net = require('net');
const http = require('http');
const aedes = require('aedes')();
const ws = require('ws');

const ACCOUNTS = process.env.ACCOUNTS_FILE || '/opt/iot-game/accounts.json';
const STATE = process.env.STATE_FILE || '/opt/iot-game/game_state.json';
const INTERNAL_USER = process.env.MQTT_USER || 'game-server';
const INTERNAL_PASS = process.env.MQTT_PASS || '';
const MQTT_PORT = Number(process.env.MQTT_PORT || 1883);
const WS_PORT = Number(process.env.WS_PORT || 9001);

// ---------------- 账号表 ----------------
let accounts = {};
try {
  accounts = JSON.parse(fs.readFileSync(ACCOUNTS, 'utf8'));
} catch (e) {
  console.error('[init] accounts.json 读取失败，按空表启动:', e.message);
  accounts = {};
}

let saveTimer = null;
function persist() {
  if (saveTimer) return;
  saveTimer = setTimeout(() => {
    saveTimer = null;
    try {
      const tmp = ACCOUNTS + '.tmp';
      fs.writeFileSync(tmp, JSON.stringify(accounts, null, 2));
      fs.renameSync(tmp, ACCOUNTS);
    } catch (e) {
      console.error('[persist] 写回失败:', e.message);
    }
  }, 500);
}

function log(...args) {
  const t = new Date().toISOString().replace('T', ' ').slice(0, 19);
  console.log(`[${t}]`, ...args);
}

// 该组是否还没有任何进度（用于允许纠正密码）
function hasProgress(group) {
  try {
    const st = JSON.parse(fs.readFileSync(STATE, 'utf8'));
    const p = (st.players || {})[group];
    if (!p) return false;
    const score = Number(p.score || 0);
    const level = Number(p.level || 0);
    return score > 0 || level > 0;
  } catch {
    return false;
  }
}

const GROUP_RE = /^[A-Za-z0-9_-]{1,32}$/;

// ---------------- 鉴权 ----------------
aedes.authenticate = (client, username, password, done) => {
  const user = String(username || '');
  const pass = password ? password.toString() : '';

  if (user === INTERNAL_USER) {
    const ok = pass === INTERNAL_PASS;
    if (!ok) log('AUTH internal-fail', client ? client.id : '?');
    return done(ok ? null : new Error('internal auth failed'), ok);
  }

  if (!GROUP_RE.test(user)) {
    log('AUTH reject-bad-group', JSON.stringify(user), 'id=' + (client ? client.id : '?'));
    return done(new Error('invalid group id'), false);
  }

  const rec = accounts[user];
  if (!rec) {
    // 首次连接：登记
    accounts[user] = { password: pass, source: 'tofu', firstSeen: new Date().toISOString() };
    persist();
    log('AUTH tofu-register', user, 'pass=' + pass, 'id=' + (client ? client.id : '?'));
    return done(null, true);
  }

  if (rec.password === pass) {
    log('AUTH ok', user);
    return done(null, true);
  }

  // 密码不一致：如果还没开始答题，允许纠正（防止第一次算错就锁死）
  if (!hasProgress(user)) {
    log('AUTH correct-no-progress', user, `${rec.password} -> ${pass}`);
    rec.password = pass;
    rec.source = 'tofu-corrected';
    rec.updatedAt = new Date().toISOString();
    persist();
    return done(null, true);
  }

  log('AUTH reject-mismatch', user, 'expect=' + rec.password, 'got=' + pass,
      'id=' + (client ? client.id : '?'));
  return done(new Error('bad password'), false);
};

aedes.on('client', (c) => log('CONNECT', c.id));
aedes.on('clientDisconnect', (c) => log('DISCONNECT', c.id));
aedes.on('clientError', (c, err) => log('ERROR', c && c.id, err.message));

// ---------------- 传输层 ----------------
const mqttServer = net.createServer(aedes.handle);
mqttServer.listen(MQTT_PORT, '0.0.0.0', () => log('MQTT 监听', MQTT_PORT));

const httpServer = http.createServer();
const wss = new ws.Server({ server: httpServer });
wss.on('connection', (socket) => {
  const stream = ws.createWebSocketStream(socket);
  aedes.handle(stream);
});
httpServer.listen(WS_PORT, '0.0.0.0', () => log('WebSocket 监听', WS_PORT));

process.on('SIGTERM', () => {
  log('收到 SIGTERM，退出');
  mqttServer.close();
  httpServer.close();
  aedes.close(() => process.exit(0));
});
