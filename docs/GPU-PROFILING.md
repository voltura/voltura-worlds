# GPU stage profiling

Run a native capture with `--gpu-stages`, for example:

```powershell
python scripts/test_desktop.py -- build/Release/VolturaWorlds.exe --capture-dir artifacts/profile --scene smoke --width 1920 --height 1080 --frames 720 --grid 64 --gpu-stages --debug-gpu
```

`metrics.json` contains whole-frame timing and per-stage sample counts, mean milliseconds, and 95th percentiles. Pressure timing sums all iterations in each frame before calculating statistics. Stage queries are disabled in normal desktop operation. Results are consumed only after the corresponding frame fence completes.

Do not sum stage percentiles. The scopes omit some command gaps, and timestamps add overhead. Captures measure offscreen GPU work, not display latency, power, or sustained presentation rate. The initial eleven submitted frames are excluded, but the remaining sample window still includes the evolving simulation; it is not a fixed mature-state benchmark.

Smoke currently stores conservative occupancy flags in the lighting volume's alpha channel. Four-cell bricks include a one-cell halo for trilinear filtering. Empty-space traversal preserves the original sample positions in occupied regions. `--no-empty-skip` disables traversal skipping for comparisons; it does not disable occupancy generation, so that control is not the old renderer's total cost.

Native regressions:

- `gpu_stage_contracts`: profiling leaves PNG output identical; stage totals fit inside the enclosing frame timing.
- `smoke_empty_space_contracts`: skipping preserves PNG output for sparse, mature, and rectangular fields.

Revision 93's matched 1080p captures reduced average scene drawing from 4.71 ms to 2.81 ms with identical PNG output. Whole-frame performance and visual quality remain below acceptance targets. These results justify investigating how to spend the saved rendering time on finer structure; they do not establish that finer structure has been delivered.
