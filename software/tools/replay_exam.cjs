'use strict';

const assert = require('node:assert/strict');
const { spawn } = require('node:child_process');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const readline = require('node:readline');
const { once } = require('node:events');
const { makeProfile } = require('./make_demo_answers.cjs');

async function withHandheld(binary, profile, run) {
  const args = profile ? [profile] : [];
  const child = spawn(binary, args, { stdio: ['pipe', 'pipe', 'inherit'] });
  const closed = once(child, 'close');
  const lines = readline.createInterface({ input: child.stdout })[Symbol.asyncIterator]();
  async function input(op, params = {}) {
    child.stdin.write(JSON.stringify({ op, ...params }) + '\n');
    const line = await lines.next();
    assert.equal(line.done, false, 'The firmware simulator ended before replying');
    return JSON.parse(line.value);
  }
  try {
    return await run(input);
  } finally {
    child.stdin.end();
    const [code] = await closed;
    assert.equal(code, 0, 'The firmware simulator failed');
  }
}

async function main() {
  const [, , binary, source, ...extra] = process.argv;
  if (!binary || !source || extra.length) {
    throw new Error('Usage: node replay_exam.cjs <built core_harness.exe> <local server source directory>');
  }
  const server = path.resolve(source);
  const { createGame } = require(path.join(server, 'game-engine.js'));
  const bank = require(path.join(server, 'questions.js'));
  const questions = new Map(bank.flatMap(level => level.questions).map(q => [q.qid, q]));
  const temp = fs.mkdtempSync(path.join(os.tmpdir(), 'h2o-exam-'));
  const stateFile = path.join(temp, 'state.json');
  const exhaustiveState = path.join(temp, 'all-questions.json');
  const profile = path.join(temp, 'demo.json');
  const mismatched = path.join(temp, 'changed.json');
  const { json, count } = makeProfile(bank);
  fs.writeFileSync(profile, json, { mode: 0o600 });
  const changed = JSON.parse(json);
  for (const row of changed.answers) row[1] = (row[1]+1) >>> 0;
  fs.writeFileSync(mismatched, JSON.stringify(changed), { mode: 0o600 });

  let game = createGame({ questionBank: bank, stateFile, drawTarget: 20 });
  const { total_draw: draw, max_score: maximum } = game.getMeta();
  assert.equal(draw, 20);
  assert.equal(maximum, 200);

  const send = (group, { type, data }) => JSON.parse(game.handleMessage(group, type, data).reply);
  const open = async (input, group) => {
    await input('link', { up: true });
    const hello = send(group, { type: 'hello', data: { protocol: 2, name: 'Local simulation' } });
    await input('receive', { payload: hello });
    const { command } = await input('resume');
    const state = await input('receive', { payload: send(group, command) });
    assert.equal(state.statusKnown, true);
    return state;
  };

  try {
    await withHandheld(path.resolve(binary), null, async input => {
      const group = 'LOCAL-MANUAL';
      let state = await open(input, group);
      assert.equal(state.hp, 5);
      const step = async (op, args) => {
        const { command } = await input(op, args);
        assert.ok(command, op);
        return input('receive', { payload: send(group, command) });
      };
      state = await step('start');
      while (state.phase === 'story') state = await step('advance');
      assert.equal(state.phase, 'question');
      await input('choose', { index: 0 });
      assert.equal((await input('clear')).selected, 0);
      assert.equal((await input('submit')).command, null);
      const question = questions.get(state.qid);
      for (const key of question.answer.split(',').map(key => key.trim())) {
        await input('choose', { index: question.options.findIndex(option => option.key === key) });
      }
      const { command } = await input('submit');
      const judge = send(group, command);
      assert.equal(judge.type, 'judge');
      assert.equal(judge.data.correct, true);
      assert.deepEqual(send(group, command), judge);
      await input('link', { up: false });
      game = createGame({ questionBank: bank, stateFile, drawTarget: 20 });
      state = await open(input, group);
      assert.equal(state.phase, 'judge');
      assert.equal(state.score, 10);
      assert.equal(state.hp, 5);
      assert.ok(['story', 'question'].includes((await step('advance')).phase));
      console.log('Manual selection, cancellation, server scoring, idempotency and reconnect: PASS');
    });

    const uniqueQuestions = new Set();
    for (let run = 1; run <= 7; run++) {
      const exhaustive = run === 7;
      if (exhaustive) game = createGame({ questionBank: bank, stateFile: exhaustiveState, drawTarget: count });
      await withHandheld(path.resolve(binary), profile, async input => {
        const group = `LOCAL-AUTO-${run}`;
        await open(input, group);
        let state = await input('demo_start', { now: 1000 });
        assert.equal(state.demoActive, true);
        const answered = new Set();
        for (let now = 5000; state.demoActive && now <= 3e6; now += 4000) {
          state = await input('demo_tick', { now });
          if (!state.command) continue;
          const response = send(group, state.command);
          assert.notEqual(response.type, 'error');
          if (state.command.type === 'answer') {
            assert.equal(response.type, 'judge');
            assert.equal(response.data.correct, true);
            answered.add(state.command.data.qid);
            uniqueQuestions.add(state.command.data.qid);
          }
          state = await input('receive', { payload: response });
        }
        assert.equal(state.phase, 'finished');
        assert.equal(state.demoActive, false);
        assert.equal(answered.size, exhaustive ? count : draw);
        assert.equal(state.score, exhaustive ? count*10 : maximum);
      });
    }
    assert.equal(uniqueQuestions.size, count);
    console.log(`F1 on 6 server draws: ${draw} correct per draw, ${maximum} points each; exhaustive replay: ${count} correct out of ${count}.`);

    await withHandheld(path.resolve(binary), mismatched, async input => {
      const group = 'LOCAL-CHANGED-BANK';
      await open(input, group);
      let state = await input('demo_start', { now: 1000 });
      for (let now = 5000; state.demoActive && now <= 2e5; now += 4000) {
        state = await input('demo_tick', { now });
        if (!state.command) continue;
        assert.notEqual(state.command.type, 'answer');
        state = await input('receive', { payload: send(group, state.command) });
      }
      assert.equal(state.phase, 'question');
      assert.equal(state.demoActive, false);
      assert.equal(state.score, 0);
      console.log('Changed question fingerprint stops before any answer: PASS');
    });
  } finally {
    for (const file of [profile, mismatched, stateFile, exhaustiveState]) {
      if (fs.existsSync(file)) fs.unlinkSync(file);
    }
    fs.rmdirSync(temp);
  }
}

main().catch(error => {
  console.error(error.message);
  process.exitCode = 1;
});
