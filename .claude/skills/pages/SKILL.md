---
name: pages
description: House style for quant-platform's standalone HTML pages, the lane write-ups under research/ and the top-level roadmap.html. Load before writing or editing either.
---

Two kinds of page share one base. Prose follows the `writing` skill and, for research, the `research` skill's voice. Terse, no em dashes, no colons or semicolons in prose.

## Base

- One `.html` per page. Fonts from Google Fonts only, Fraunces for headings, IBM Plex Sans for body, IBM Plex Mono for labels and numbers. KaTeX from jsDelivr where there is math.
- Colours are tokens on `:root`, redefined under `@media (prefers-color-scheme: dark)` guarded by `:root:not([data-theme="light"])` and again under `:root[data-theme="dark"]`. `body` sets its background.
- One column, `max-width` 680 to 720px, 16px side gutter, no horizontal scroll at phone width. Wide tables sit in a `.scroll` wrapper.
- A cover that fills the first screen. Mono eyebrow, a short accent bar, a Fraunces title, one-line sub, a `dl.meta` block.
- Sections numbered with a mono `.num`, `h2` in Fraunces, `h3` in Plex Sans 600.
- Tables are mono headers, hairline rules, no fills.
- Charts are PNGs in an `img/` folder beside the page, white background, a `figcaption` under each.

## Lane write-up

`research/signal/<lane>/<lane>.html`. Single purple accent. Section order is set by the `research` skill's closing-a-lane steps. A lane opens with the same page as its plan, rewritten into the write-up at close.

- Styling lives once in `research/signal/page.css`, linked as `../page.css`. No `<style>` block in a lane page. A new class goes in the shared sheet.
- `.overview` for the summary card, `.corrected` for a number as it stood with its later correction, `.think` for a question to settle before building.
- Each number as it stood on the day. A correction never silently replaces it.

## Roadmap

`roadmap.html` at the repo root. What is being done and what is next, terse, linking to the detail rather than repeating it.

- A slowly turning multi-coloured spiral fixed behind the page, inline SVG, one arm colour per area, off under `prefers-reduced-motion` and in print.
- Colour code by area. Signal purple, regime teal, strategy blue, engineering orange, data green. A section card takes its area's colour on its left border, its `.num` and its `.tag`s.
- `.item` for an open piece of work, `.think` for a question to settle before building.
- Update the "as of" date and the state table on every edit.
