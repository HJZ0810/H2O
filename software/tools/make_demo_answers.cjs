'use strict';

const fs = require('node:fs');
const path = require('node:path');

function fingerprint(question) {
  let hash = 2166136261;
  const parts = [question.qid, question.type, question.text];
  for (const { key, text } of question.options) parts.push(key, text);
  for (const part of parts) {
    for (const byte of Buffer.from(part, 'utf8')) hash = Math.imul(hash ^ byte, 16777619);
    hash = Math.imul(hash, 16777619);
  }
  return hash >>> 0;
}

function makeProfile(bank) {
  const seen = new Set();
  const answers = bank.flatMap(level => level.questions).map(question => {
    const optionKeys = question.options.map(option => option.key);
    const choices = String(question.answer).toUpperCase().split(',').map(key => key.trim()).sort();
    if (!/^[A-Za-z0-9_-]{1,64}$/.test(question.qid) || seen.has(question.qid) ||
        !['AB', 'ABC', 'ABCD'].includes(optionKeys.join('')) ||
        new Set(choices).size !== choices.length || choices.some(key => !optionKeys.includes(key)) ||
        (question.type !== 'multi' && choices.length !== 1)) {
      throw new Error('The question bank contains a duplicate ID or unsupported answer');
    }
    seen.add(question.qid);
    return [question.qid, fingerprint(question), choices.join(',')];
  });
  const json = JSON.stringify({ version: 1, answers }) + '\n';
  if (!answers.length || Buffer.byteLength(json) > 3900) throw new Error('The profile exceeds the firmware format limits');
  return { json, count: answers.length };
}

if (require.main === module) {
  const [, , source, destination] = process.argv;
  if (!source || !destination) {
    console.error('Usage: node make_demo_answers.cjs <authorized local questions.js> <private output.json>');
    process.exit(2);
  }
  const { json, count } = makeProfile(require(path.resolve(source)));
  fs.writeFileSync(path.resolve(destination), json, { flag: 'wx', mode: 0o600 });
  console.log(`Prepared ${count} answers for local device setup.`);
}

module.exports = { makeProfile };
