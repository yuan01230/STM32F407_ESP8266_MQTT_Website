# Industrial Control Room Restyle Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Restyle the existing IoT dashboard into an industrial control room interface without changing any functionality.

**Architecture:** Keep the current HTML structure and JavaScript hooks intact, and concentrate all visual changes in the stylesheet. Reuse the existing class names so the behavior layer remains untouched while the presentation layer is fully redesigned.

**Tech Stack:** Static HTML, CSS, vanilla JavaScript, Express-served frontend

---

### Task 1: Capture the approved visual direction

**Files:**
- Create: `docs/superpowers/specs/2026-05-24-industrial-control-room-design.md`
- Create: `docs/superpowers/plans/2026-05-24-industrial-control-room-restyle.md`

- [ ] Step 1: Record the approved constraints and theme direction in a design note
- [ ] Step 2: Record the implementation approach in this plan file

### Task 2: Rewrite the visual system in CSS

**Files:**
- Modify: `client/style.css`

- [ ] Step 1: Replace the neon space palette with an industrial control room token set
- [ ] Step 2: Restyle layout, sidebar, header, cards, controls, terminal, drawers, and responsive behavior
- [ ] Step 3: Preserve all existing selectors required by `client/app.js`

### Task 3: Verify compatibility

**Files:**
- Modify: `client/style.css`

- [ ] Step 1: Reload the served page through the local Express server
- [ ] Step 2: Verify homepage and API endpoints still respond
- [ ] Step 3: Confirm the redesign did not require JavaScript changes
