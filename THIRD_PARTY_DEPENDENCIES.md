# Third-party dependencies and license notices

Aetherclime uses open-source libraries resolved via its pinned vcpkg registry. The following **local Windows build environment** was audited for release candidate 0.5.0 RC1 (`x64-windows-static-md`):

| Package | Resolved version | License | Acknowledgment |
|---|---|---|---|
| CommonLibSSE-NG | 3.7.0 (older CharmedBaryon/Ryan-rsm-McKenzie line) | MIT | Ryan-rsm-McKenzie and CommonLibSSE-NG contributors |
| nlohmann-json | 3.12.0 | MIT | Niels Lohmann |
| spdlog | 1.17.0 | MIT | Gabi Melman and contributors |
| fmt | 12.2.0 | MIT | Victor Zverovich and contributors |
| rapidcsv | 9.07 | BSD 3-Clause | Kristofer Berggren |
| xbyak | 7.28 | BSD-style 3-clause | MITSUNARI Shigeo |

**Note about CommonLib:** License details vary between CommonLibSSE-NG forks and revisions. The release audit verified that the locally installed **3.7.0** copyright file is MIT-licensed. Do not assume a different resolved CommonLib version has the same license.

**Source vs binary:** This source repository **does not vendor** the above dependency libraries. They are fetched by vcpkg. The distributed Windows binary archive must preserve the exact relevant copyright/license/disclaimer notices for statically linked dependencies. The Windows release packaging script checks for those original installed vcpkg copyright files and places them into the FOMOD as `Documentation/ThirdPartyLicenses/` and a summary `THIRD_PARTY_NOTICES.md`.

**SKSE64** is a separately installed runtime requirement and is not included in this repository's runtime installer. Skyrim game assets are not covered by Aetherclime's MIT license.

Aetherclime's original source license is in [`LICENSE`](LICENSE). No third-party dependency license is superseded by the original project's license.
