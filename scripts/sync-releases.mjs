#!/usr/bin/env node
// scripts/sync-releases.mjs
// Maintain release-notes/_boards.json — a map of { "<tag>": [boardId, ...] }
// the install page uses to filter the board picker per firmware version.
//
// Two modes:
//
//   default                       Pulls every release from the GitHub API and
//                                 rebuilds the map from each release's .bin
//                                 asset names. Also seeds release-notes/<tag>.md
//                                 from the GH release body when missing (never
//                                 overwrites). Run after CI publishes a release.
//
//   --seed <version>              No network. Reads .github/workflows/release.yml,
//                                 extracts the strategy.matrix.env list (the
//                                 boards that will ship), merges that entry into
//                                 the existing _boards.json. Run before tagging
//                                 a new release so the map is in the tag commit.
//
// Usage:
//   node scripts/sync-releases.mjs                # full GitHub sync
//   node scripts/sync-releases.mjs --quiet        # silence info logs
//   node scripts/sync-releases.mjs --dry-run      # don't write files, just report
//   node scripts/sync-releases.mjs --seed 1.9.0   # local seed from workflow matrix

import { existsSync, mkdirSync, readFileSync } from 'node:fs';
import { writeFile } from 'node:fs/promises';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const here     = dirname(fileURLToPath(import.meta.url));
const repoRoot = join(here, '..');
const notesDir = join(repoRoot, 'release-notes');
const mapFile  = join(notesDir, '_boards.json');
const workflow = join(repoRoot, '.github', 'workflows', 'release.yml');

const REPO    = 'lshaf/unigeek';
const QUIET   = process.argv.includes('--quiet');
const DRY_RUN = process.argv.includes('--dry-run');
const log     = (m) => { if (!QUIET) process.stderr.write(`[sync] ${m}\n`); };

const ASSET_RE = /^unigeek-(.+)\.bin$/i;

function parseSeedFlag() {
  const i = process.argv.indexOf('--seed');
  if (i === -1) return null;
  const v = process.argv[i + 1];
  if (!v || v.startsWith('--')) {
    throw new Error('--seed requires a version argument, e.g. --seed 1.9.0');
  }
  return v.replace(/^v/i, '');
}

function loadMap() {
  if (!existsSync(mapFile)) return {};
  try {
    return JSON.parse(readFileSync(mapFile, 'utf8'));
  } catch (err) {
    throw new Error(`could not parse ${mapFile}: ${err.message}`);
  }
}

function readMatrixBoardsFromWorkflow() {
  if (!existsSync(workflow)) throw new Error(`workflow not found: ${workflow}`);
  const text = readFileSync(workflow, 'utf8');
  // Locate the strategy.matrix.env block; capture lines until the block ends
  // (next sibling key, or `steps:` / end of strategy).
  const m = text.match(/strategy:[\s\S]*?matrix:\s*\n\s*env:\s*\n([\s\S]*?)(?=\n\s*\w[\w-]*:|\n\s*steps:)/);
  if (!m) throw new Error('could not locate strategy.matrix.env in release.yml');
  const ids = [];
  for (const line of m[1].split('\n')) {
    const bm = line.match(/^\s*-\s+([\w_-]+)\s*$/);
    if (bm) ids.push(bm[1]);
  }
  if (ids.length === 0) throw new Error('matrix.env block has no entries');
  return ids.sort();
}

function compareVersionsDesc(a, b) {
  const pa = a.split('.').map(Number);
  const pb = b.split('.').map(Number);
  for (let i = 0; i < 3; i++) {
    if ((pb[i] ?? 0) !== (pa[i] ?? 0)) return (pb[i] ?? 0) - (pa[i] ?? 0);
  }
  return 0;
}

function sortMap(map) {
  const sorted = {};
  for (const tag of Object.keys(map).sort(compareVersionsDesc)) {
    sorted[tag] = map[tag];
  }
  return sorted;
}

async function writeMap(map) {
  if (DRY_RUN) {
    log(`would write ${mapFile} with ${Object.keys(map).length} entries`);
    process.stdout.write(JSON.stringify(map, null, 2) + '\n');
    return;
  }
  mkdirSync(notesDir, { recursive: true });
  await writeFile(mapFile, JSON.stringify(map, null, 2) + '\n', 'utf8');
  log(`wrote release-notes/_boards.json (${Object.keys(map).length} versions)`);
}

async function fetchAllReleases() {
  const url = `https://api.github.com/repos/${REPO}/releases?per_page=100`;
  const res = await fetch(url, {
    headers: {
      accept: 'application/vnd.github+json',
      'user-agent': 'unigeek-sync-releases',
    },
  });
  if (!res.ok) {
    const body = await res.text().catch(() => '');
    throw new Error(`GitHub API ${res.status} ${res.statusText} :: ${body.slice(0, 200)}`);
  }
  return res.json();
}

async function ensureReleaseNote(tag, body) {
  const path = join(notesDir, `${tag}.md`);
  if (existsSync(path)) return false;
  if (DRY_RUN) {
    log(`would write release-notes/${tag}.md`);
    return true;
  }
  mkdirSync(notesDir, { recursive: true });
  await writeFile(path, `# ${tag}\n\n${(body || '').trim()}\n`, 'utf8');
  log(`wrote release-notes/${tag}.md`);
  return true;
}

async function runSeed(version) {
  log(`seeding ${version} from release.yml matrix`);
  const boards = readMatrixBoardsFromWorkflow();
  log(`  matrix: ${boards.length} boards`);

  const map = loadMap();
  const prev = map[version];
  if (prev && JSON.stringify(prev) === JSON.stringify(boards)) {
    log(`${version} already up-to-date in _boards.json`);
    return;
  }
  map[version] = boards;
  await writeMap(sortMap(map));
}

async function runSync() {
  log(`querying ${REPO} on GitHub...`);
  const releases = await fetchAllReleases();
  log(`found ${releases.length} releases`);

  const map = {};
  let created = 0;

  for (const r of releases) {
    const tag = r.tag_name.replace(/^v/i, '');
    const ids = (r.assets || [])
      .map((a) => {
        const m = a.name.match(ASSET_RE);
        return m ? m[1] : null;
      })
      .filter(Boolean)
      .sort();
    if (ids.length === 0) {
      log(`  ${tag}: no .bin assets`);
      continue;
    }
    map[tag] = ids;
    log(`  ${tag}: ${ids.length} boards`);
    if (await ensureReleaseNote(tag, r.body)) created++;
  }

  await writeMap(sortMap(map));
  if (created > 0) log(`seeded ${created} new release-notes/*.md files`);
}

async function main() {
  const seedVersion = parseSeedFlag();
  if (seedVersion) {
    await runSeed(seedVersion);
  } else {
    await runSync();
  }
}

main().catch((err) => {
  process.stderr.write(`[sync] ERROR: ${err.message}\n`);
  process.exit(1);
});
