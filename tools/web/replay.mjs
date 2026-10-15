// Replays playthrough recordings (docs/playthrough.md) in the web build,
// headless in Chromium and Firefox, and checks that each ends as its
// "# end:" line says, as test_play_game does for the native build.
//
//   node tools/web/replay.mjs [RECORDING...]
//
// RECORDING defaults to tests/play/proof_12345.txt (about two minutes per
// browser). Needs the web preset built (build/web) and the floppy release in
// vendor/Reunion_DOS_EN ($REUNION_DATA overrides); once, in tools/web:
// pnpm install && pnpm exec playwright install chromium firefox. $CHROMIUM
// names another Chromium to run (e.g. /usr/bin/chromium-browser where
// Playwright's own doesn't). Exit status: 0 if every replay ended as recorded.
import { readFileSync, readdirSync } from 'node:fs';
import { join, relative, resolve } from 'node:path';
import { chromium, firefox } from 'playwright';

const root = resolve(import.meta.dirname, '../..');
const web = join(root, 'build/web');
const data = process.env.REUNION_DATA || join(root, 'vendor/Reunion_DOS_EN');
const recordings = process.argv.slice(2);
if (!recordings.length)
    recordings.push(join(root, 'tests/play/proof_12345.txt'));
const files = readdirSync(data, { recursive: true, withFileTypes: true })
    .filter(f => f.isFile())
    .map(f => relative(data, join(f.parentPath, f.name)));

// The page fetches the game data and the recording into the build's
// in-memory file system, then runs the game with --replay.
const page = recording => `<!doctype html>
<canvas id="canvas"></canvas>
<script>
  window.lines = [];
  (async () => {
    const files = await Promise.all(${JSON.stringify(files)}.map(async f =>
        [f, new Uint8Array(await (await fetch('data/' + f)).arrayBuffer())]));
    window.Module = {
      canvas: document.getElementById('canvas'),
      arguments: ['--replay=/recording.txt', '/data'],
      print: text => lines.push(text),
      onAbort: what => lines.push('abort: ' + what),
      preRun: [() => {
        for (const [f, bytes] of files) {
          FS.mkdirTree('/data/' + f.slice(0, f.lastIndexOf('/') + 1));
          FS.writeFile('/data/' + f, bytes);
        }
        FS.writeFile('/recording.txt', ${JSON.stringify(readFileSync(recording, 'utf8'))});
      }],
    };
    document.body.append(Object.assign(document.createElement('script'), { src: 'reunion.js' }));
  })();
</script>`;

let failed = false;
for (const [name, type] of [['chromium', chromium], ['firefox', firefox]]) {
    const browser = await type.launch(name === 'chromium' && process.env.CHROMIUM
                                      ? { executablePath: process.env.CHROMIUM } : {});
    for (const recording of recordings) {
        const tab = await browser.newPage();
        const errors = [];
        tab.on('pageerror', e => errors.push(e.message));
        // Served from memory: the page, the build and the data.
        await tab.route('http://reunion.test/**', route => {
            const path = decodeURIComponent(new URL(route.request().url()).pathname.slice(1));
            if (path === 'replay.html')
                return route.fulfill({ contentType: 'text/html', body: page(recording) });
            return route.fulfill({ path: path.startsWith('data/') ? join(data, path.slice(5)) : join(web, path) });
        });
        const start = Date.now();
        await tab.goto('http://reunion.test/replay.html');
        let result;
        try {
            await tab.waitForFunction(() => lines.some(l => /^(replay|abort):/.test(l)), null,
                                      { timeout: 30 * 60000, polling: 1000 });
            result = (await tab.evaluate(() => lines)).find(l => /^(replay|abort):/.test(l));
        } catch (e) {
            result = errors[0] || e.message.split('\n')[0];
        }
        const expected = readFileSync(recording, 'utf8').match(/^# end: (.*)$/m)?.[1];
        const ok = expected !== undefined && result === `replay: ${expected}`;
        failed ||= !ok;
        console.log(`${name} ${relative(root, recording)}: ${result} (${Math.round((Date.now() - start) / 1000)} s)` +
                    (ok ? '' : `, expected ${expected ?? 'an "# end:" line'}`));
        await tab.close();
    }
    await browser.close();
}
process.exit(failed ? 1 : 0);
