import fs from "fs";
import path from "path";
import { marked } from "marked";
import { CATALOG, CATEGORIES, getCategory } from "./catalog";

const knowledgeDir = path.join(process.cwd(), "..", "knowledge");

// Extend marked: blockquotes starting with [!note], [!warn], [!tip], [!danger]
// become labelled callouts. Plain `>` blockquotes keep the default NOTE styling.
function createRenderer() {
  const renderer = new marked.Renderer();

  renderer.blockquote = function (quote) {
    const m = quote.match(/^\s*<p>\s*\[!(note|tip|warn|danger)\]\s*/i);
    const kind = m ? m[1].toLowerCase() : null;
    const stripped = m ? quote.replace(/^\s*<p>\s*\[![a-z]+\]\s*/i, "<p>") : quote;
    const cls = kind ? ` class="${kind}"` : "";
    return `<blockquote${cls}>${stripped}</blockquote>\n`;
  };

  // Cross-doc links use the source `slug.md` form (so they also resolve when
  // browsing the repo). On the website, rewrite a relative `slug.md` (optionally
  // with an #anchor) to its real route `/features/slug`. External/absolute
  // links pass through untouched.
  renderer.link = function (href, title, text) {
    let outHref = href || "";
    const m = /^([a-z0-9-]+)\.md(#[^)]*)?$/i.exec(outHref);
    if (m) outHref = `/features/${m[1]}${m[2] || ""}`;
    const titleAttr = title ? ` title="${title}"` : "";
    return `<a href="${outHref}"${titleAttr}>${text}</a>`;
  };

  // Body cells that contain exactly "Bronze" / "Silver" / "Gold" / "Platinum"
  // render as tier pills (same visual language as the in-app achievement list).
  renderer.tablecell = function (content, flags) {
    const tag = flags.header ? "th" : "td";
    const align = flags.align ? ` style="text-align:${flags.align}"` : "";
    const trimmed = content.trim();
    if (!flags.header && /^(Bronze|Silver|Gold|Platinum)$/.test(trimmed)) {
      const kind = trimmed.toLowerCase();
      return `<${tag}${align}><span class="tier ${kind}">${trimmed}</span></${tag}>\n`;
    }
    return `<${tag}${align}>${content}</${tag}>\n`;
  };

  return renderer;
}

export function getAllFeatures() {
  return CATALOG;
}

export function getFeaturesInCategory(categoryId) {
  return CATALOG.filter((f) => f.category === categoryId);
}

export function getFeatureBySlug(slug) {
  const entry = CATALOG.find((f) => f.slug === slug);
  if (!entry || !entry.hasDetail) return null;

  const filePath = path.join(knowledgeDir, `${slug}.md`);
  if (!fs.existsSync(filePath)) return null;

  const raw = fs.readFileSync(filePath, "utf8");
  const content = raw.replace(/^#\s+.+\n/, "");

  const renderer = createRenderer();
  const html = marked.parse(content, { renderer, headerIds: false, mangle: false });

  const category = getCategory(entry.category);

  const all = CATALOG.filter((f) => f.hasDetail);
  const idx = all.findIndex((f) => f.slug === slug);
  const prev = idx > 0 ? all[idx - 1] : null;
  const next = idx >= 0 && idx < all.length - 1 ? all[idx + 1] : null;

  return { ...entry, html, category, prev, next };
}

export function getDocSlugs() {
  return CATALOG.filter((f) => f.hasDetail).map((f) => f.slug);
}

export function getDocsByCategory() {
  const grouped = CATEGORIES.map((c) => ({
    ...c,
    features: CATALOG.filter((f) => f.category === c.id && f.hasDetail),
  })).filter((g) => g.features.length > 0);
  return grouped;
}
