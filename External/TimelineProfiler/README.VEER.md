# TimelineProfiler integration

Source: C:/Users/2250183/Downloads/TimelineProfiler-main
Original author: Simon Coenen. See LICENSE for the original license notices.

This is an altered source distribution, adapted for VEER:
- CPU event/history storage and the original timeline GUI are retained.
- D3D12 GPU profiling is removed; VEER uses D3D11.
- Symbols are isolated in the TimelineProfiler namespace.
- The existing ImGui library and fonts are reused. Bundled ImGui, font data, executable, and DX12 example are omitted.
- HUD rendering does not record live profiling events into the static demo.
- Event labels are rendered as unformatted text.

VEER Profiler/ProfileScope now publish live CPU samples at frame end. Worker samples are buffered under a mutex; the timeline backend is only modified on the main thread. The old demo entry point forwards to the live view.
- Pause/resume handling and startup frame anchors are corrected for the live view.

- Assertion/scope macros retain upstream MSVC traditional preprocessor forwarding for VEER.
- The namespaced Profiler type is explicitly forward-declared before gProfiler.
- The external core uses a unique TimelineProfilerCore.obj output name.
