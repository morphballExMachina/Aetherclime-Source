# Aetherclime 0.5.0 RC1 — Nexus permissions and licensing review

**Author decision checklist, not legal advice.** This guide is based on the repository's `LICENSE` (MIT) and current Nexus guidance. Review the live upload form before publication.

## What the repository currently grants

The root MIT license grants rights to **use, copy, modify, merge, publish, distribute, sublicense, and sell** the software and associated covered documentation, subject to including the original copyright and license notice. It does not grant rights to game assets or third-party assets that were not licensed by the Aetherclime maintainer.

Therefore, Nexus permission selections that prohibit all redistribution, modification, or asset reuse without prior permission may **contradict the repository's stated MIT license** when applied to the same covered software. The final source license and platform permissions should express a coherent policy.

## Review these Nexus fields before publishing

| Nexus setting | Recommended alignment for original Aetherclime MIT code |
|---|---|
| Upload / redistribution | Allow where MIT permits, provided the copyright and license are included; do not authorize uploading unrelated third-party assets |
| Modification | Allow modifications of MIT-covered source with attribution/license notice |
| Conversion to another game | Depends on game assets and runtime feasibility; the MIT code itself permits adaptation |
| Asset use | Allow reuse of Aetherclime-owned MIT software; no claim over SKSE, CommonLib or others' artwork |
| Forks / parallel versions | Explicitly decide whether to allow Nexus-hosted forks; align that decision with the MIT source license and the platform's separate moderation rules |
| AI training / AI-tool workflows | **Separate deliberate choice:** Nexus provides a distinct control. Do not assume MIT answers every platform-specific AI workflow question; choose it knowingly and disclose the choice |
| Donation Points / commercial reuse | MIT permits commercial use of covered software. Check Nexus's separate Donation Points policies and third-party licensing before selecting permissions |
| Credits | Retain MIT copyright and permission notice; prominently credit third-party contributions and assets |

Do **not** promise that a user may redistribute Bethesda, SKSE, CommonLibSSE-NG, graphics, screenshots or other authors' files just because Aetherclime is MIT-licensed.

## AI disclosure

- **AI-Generated Content:** Appropriate because substantial source implementation was generated with AI.
- **AI Media:** Appropriate for AI-authored promotional imagery and Nexus-page descriptions, where applicable; the page copy was AI-assisted/generated.
- Include the short factual development-transparency paragraph already present in the Nexus page. Do not mislabel the project as only limited **AI Assisted** development.
- Review the AI-training/forking controls separately from code copyright and ensure any externally sourced files were used with appropriate permissions.

## Pre-publication sign-off

- [ ] Confirm the complete original-code/license provenance (including contributor-authored files, generated artwork, embedded data, and third-party dependencies).
- [ ] Ensure Nexus **Permissions and Credits** controls match the intended MIT source licensing and do not imply rights over external assets.
- [ ] Set the Nexus AI usage classification and tag/page-media disclosure accurately.
- [ ] Add links to any third-party source or dependency when required by its license and Nexus attribution guidance.
- [ ] Confirm actual files distributed contain the MIT license and no unnecessary development data.

Current Nexus policies: https://help.nexusmods.com/article/28-file-submission-guidelines and https://help.nexusmods.com/article/180-ai-policy-changes-2026 (reviewed November 25, 2026).
