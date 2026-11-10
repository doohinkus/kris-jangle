// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 Charles Vestal
// Boot the emulator's app.wasm and exit: the test that runs this (tests/sysex_web_test.mjs) uses it to catch a boot
// that blocks. A sleep in the emulator never ends (its clock does not move in the AudioWorklet), so the boot has to
// come back; here it runs in a process of its own, with a timeout, so a boot that blocks is a failure and not a
// stuck test.
//   node tests/emu_boot_test.mjs DIR
import { readFileSync } from "node:fs";

const dir = process.argv[2];
if (!dir) {
  console.error("usage: emu_boot_test.mjs DIR");
  process.exit(2);
}
let mem;
const wasi = new Proxy({
  clock_time_get: (id, prec, out) => { new DataView(mem.buffer).setBigUint64(out, 0n, true); return 0; },
  fd_write: (fd, iov, n, out) => { new DataView(mem.buffer).setUint32(out, 0, true); return 0; },
}, { get: (t, k) => (k in t ? t[k] : () => 0) });
const { instance } = await WebAssembly.instantiate(readFileSync(`${dir}/app.wasm`),
  { wasi_snapshot_preview1: wasi, env: new Proxy({}, { get: () => () => 0 }) });
mem = instance.memory;
if (instance.exports._initialize) instance.exports._initialize();
instance.exports.web_boot();
console.log("booted");
