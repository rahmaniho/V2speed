# Skill Resource Audit — `rahmaniho/Skill`

**Audited revision:** `d712250d6b2378f3d32371fda2177518b8f7c9d9` (2026-11-13).
**Outcome:** no skills imported or reused. Repository was inspected before deciding applicability.

## Repository review

- `README.md`: project is a Persian lead-generation product built around Google Maps scraping and includes substantial product-specific guidance; not a VPN engineering reference.
- `.agents/skills/` and `agent/skills/`: only `google-maps-scraper/SKILL.md` present in the inspected tree. This describes using a Google Maps scraping kit, its local Docker REST API, output schemas and enrichment. It is unrelated to V2Speed.
- `.claude/`: no reusable engineering skill in the checkout; skill lock metadata refers to the same Maps scraper path in a different layout.
- `skills-lock.json`: only locked skill is `google-maps-scraper` from `mahanaicoach/google-maps-scraper-kit`; it is not applicable.
- `docs/`: application/cloud operational guides for the lead-generation service, not reusable mobile engineering process.
- `tests/`: Python unittest suite and application/API/UI smoke coverage. These show examples of deterministic fixtures and regression tests but are application-specific; they are not imported as code or treated as an executed skill.
- `.github/workflows/`: Python/Node application CI, web deployment and worker jobs; no V2Speed build workflow to reuse.

## Reuse register

| Original path | Purpose | Applicability | Dependencies | Modifications | Reason |
|---|---|---|---|---|---|
| None | — | — | — | — | No relevant reusable skill was present. Google Maps scraper skill explicitly concerns unrelated data acquisition and is not imported. |

## Requested skill availability

The named `tdd-workflow`, `verification-loop`, `test-collaboration`, `codehealth-mcp`, `ai-regression-testing`, `e2e-testing`, benchmark/performance skills, `silent-failure-hunter`, and language-specific skills were not present in the inspected Skill repository. Agent slash commands/agents named in the request are not available as executable tooling in this workspace. They are not claimed as run. Where useful, their requested practices are captured in `TEST_STRATEGY.md`, the PRD and milestone verification checklist and will be used manually. Language-specific skills are only relevant if that language is actually adopted.

No Google Maps scraper functionality has been used or imported.
