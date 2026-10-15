/*
 * Driver for lv_font_conv (tools/fonts/fonts.py).
 *
 * The CLI of lv_font_conv starts one process per size; the pipeline needs the
 * generated file for ~20 sizes per candidate plus a metric probe of every size,
 * so this driver loads the font once and runs the same conversion in-process.
 * It is the same library the committed families came from (the pinned version in
 * tools/fonts/.cache/npm), driven through its programmatic API.
 *
 * Usage: node lvfc.js <spec.json>
 *
 *   { "fonts": [ { "path": "Inter[opsz,wght].ttf", "source": "…/Inter[opsz,wght].ttf",
 *                  "symbols": " !\"#…" },
 *                { "path": "fa-solid-900.ttf", "source": "…/fa-solid-900.ttf",
 *                  "ranges": ["58715,61560"] } ],
 *     "mode": "probe", "from": 5, "to": 71 }
 *
 *   { …same "fonts"…, "mode": "generate",
 *     "jobs": [ { "size": 13, "output": "…/sony_20.c" } ] }
 *
 * `path` is what lands in the header lv_font_conv writes (the upstream file name,
 * as the committed families do); `source` is where the bytes are.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

'use strict';

const fs = require('fs');
const path = require('path');

const MODULE_DIR = path.resolve(process.argv[2]);
const spec = JSON.parse(fs.readFileSync(process.argv[3], 'utf8'));

const convert = require(path.join(MODULE_DIR, 'lib', 'convert'));

function unicodePoints(value) {
  // Same syntax as the CLI's --range: `a,b,c-d`, all decimal or 0x-prefixed.
  return value.split(',').map(item => {
    const m = /^(?:(?:0x([0-9a-f]+))|(\d+))(?:-(?:(?:0x([0-9a-f]+))|(\d+)))?$/i.exec(item.trim());
    if (!m) throw new Error(`${item} is not a valid range`);
    const start = m[1] ? parseInt(m[1], 16) : parseInt(m[2], 10);
    const end = m[3] ? parseInt(m[3], 16) : m[4] ? parseInt(m[4], 10) : start;
    const flat = [];
    for (let code = start; code <= end; code++) flat.push(code, code, code);
    return flat;
  }).flat();
}

function fontEntries() {
  return spec.fonts.map(entry => {
    const ranges = [];
    if (entry.symbols) ranges.push({ symbols: entry.symbols });
    for (const range of entry.ranges || []) ranges.push({ range: unicodePoints(range) });
    return {
      source_path: entry.path,
      source_bin: fs.readFileSync(entry.source),
      ranges,
    };
  });
}

function glyphBoxH(data, char) {
  // The `.box_h` of one ASCII code point: glyph id 1 is the space (32), so the
  // descriptor index is 1 + (code - 32) with id 0 reserved.
  const text = data.toString('utf8');
  const descriptor = text.split('glyph_dsc[] = {').slice(-1)[0];
  const boxes = [...descriptor.matchAll(/\.box_h = (\d+)/g)].map(m => Number(m[1]));
  const index = 1 + (char.codePointAt(0) - 32);
  return boxes[index] ?? 0;
}

function optsString(size, output) {
  // What the CLI would have written in the file header.
  const parts = ['--bpp 4', `--size ${size}`, '--no-compress'];
  for (const entry of spec.fonts) {
    parts.push(`--font ${entry.path}`);
    if (entry.symbols) parts.push(`--symbols ${entry.symbols}`);
    for (const range of entry.ranges || []) parts.push(`--range ${range}`);
  }
  if (spec.lv_fallback) parts.push(`--lv-fallback ${spec.lv_fallback}`);
  if (output) parts.push(`--format lvgl -o ${path.basename(output)}`);
  return parts.join(' ');
}

async function run(size, output) {
  return convert({
    size,
    bpp: 4,
    compress: false,
    lcd: false,
    use_color_info: false,
    no_prefilter: false,
    force_fast_kern_format: false,
    no_kerning: false,
    format: 'lvgl',
    full_info: false,
    lv_fallback: spec.lv_fallback || null,
    // The lvgl writer only uses `output` as the file-name key; the probe mode
    // passes a throwaway name and never writes it.
    output: output || spec.probe_output || 'probe.c',
    lv_include: null,
    lv_font_name: null,
    opts_string: optsString(size, output),
    font: fontEntries(),
  });
}

function parse(out) {
  const text = out.toString('utf8');
  const lineHeight = /\.line_height = (\d+)/.exec(text);
  const baseLine = /\.base_line = (-?\d+)/.exec(text);
  return { line_height: Number(lineHeight[1]), base_line: Number(baseLine[1]) };
}

(async () => {
  if (spec.mode === 'probe') {
    for (let size = spec.from; size <= spec.to; size++) {
      const files = await run(size, null);
      const data = Object.values(files)[0];
      const { line_height, base_line } = parse(data);
      const ink = glyphBoxH(data, spec.probe_symbol || 'A');
      process.stdout.write(`${size} ${line_height} ${base_line} ${ink}\n`);
    }
    return;
  }

  if (spec.mode === 'generate') {
    for (const job of spec.jobs) {
      const files = await run(job.size, job.output);
      const [name, data] = Object.entries(files)[0];
      fs.mkdirSync(path.dirname(name), { recursive: true });
      fs.writeFileSync(name, data);
    }
    return;
  }

  throw new Error(`unknown mode ${spec.mode}`);
})().catch(err => {
  process.stderr.write(`lvfc: ${err.message}\n`);
  process.exit(1);
});
