# Third-party notices

Dependencies are fetched during CMake configuration rather than vendored here.
Their upstream copyright and license files remain authoritative.

| Dependency | Version | License / source |
| --- | --- | --- |
| JUCE | 9.0.0 | AGPLv3 or commercial license; [repository](https://github.com/juce-framework/JUCE/tree/9.0.0) |
| libebur128 | 1.2.6 | MIT; [COPYING](https://github.com/jiixyj/libebur128/blob/v1.2.6/COPYING) |
| Catch2 | 3.15.3 | Boost Software License 1.0; [repository](https://github.com/catchorg/Catch2/tree/v3.15.3) |
| REAPER SDK | pinned in bridge/CMakeLists.txt | Upstream SDK notices; [repository](https://github.com/justinfrankel/reaper-sdk) |

JUCE includes additional third-party components with their own notices. Review
the dependency source distributions when preparing a binary release. REAPER is
a separate product and is not distributed with MixDebugger.
