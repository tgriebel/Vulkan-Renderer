# Depth of Field Algorithms — Survey & Jimenez Deep Dive

## Part 1 — Algorithm survey (physically plausible, easy-ish)

All of these use a thin-lens **CoC** (circle of confusion):

```
CoC_pixels ≈ (f/N) * (Z - S)/Z * f/(S - f) * pxPerMeter
```

where `f` = focal length, `N` = f-stop, `S` = focus distance, `Z` = linear depth.

### 1. Mip-chain DoF (the "simple fix" approach)
Sample a pre-blurred mip pyramid at `mip = |CoC| * k`. Trivial to add far-field blur and use a proper CoC. Cheap. Bleeds across depth discontinuities because mip filtering ignores CoC.
**Good for:** quick upgrade without new passes.

### 2. Separable Gaussian DoF
Two passes (H then V), tap offsets scaled by CoC. Not physical (Gaussian ≠ bokeh) but very cheap and clean.
**Good for:** stylized / "cinematic smooth."

### 3. Jimenez / COD:AW "Next-Gen Post-Processing" separable gather — **industry sweet spot**
Half-res, two layers (near + far). Downsample packs color + CoC. Near pre-multiplied by CoC and dilated; far gather rejects taps with smaller CoC than center. Separable blur. Handles both foreground bleed and background bleed correctly.
**Good for:** physically plausible, moderate effort, reuses bloom-style infrastructure. This is what most modern engines ship.

### 4. Thin-lens gather with disk/hex kernel (bokeh gather)
Full-res or half-res gather using ~32 fixed disk taps weighted by CoC. Produces real bokeh shapes (circles/hexagons). Needs a tile-based max-CoC prepass to bound tap radius, otherwise undersamples.
**Good for:** pretty bokeh highlights; more effort than #3.

### 5. McIntosh separable hex/circular bokeh
Separates a hex bokeh into 3 skewed box blurs; near-bokeh quality at separable cost. Clever but finicky to implement correctly.
**Good for:** shaped bokeh on a tight budget.

### 6. Scatter-as-gather / sprite scatter bokeh
For each bright pixel, splat a bokeh sprite sized by CoC. Beautiful highlights, expensive, hard to composite with the rest of the scene.
**Good for:** hero shots, not general DoF.

### 7. Ray-traced / path-traced thin lens
Ground truth: jitter ray origins over the aperture disk. Only viable if you're already doing RT; otherwise irrelevant.
**Good for:** reference.

### Rule-of-thumb recommendation
- Want biggest quality jump for least work → **#1 done properly** (physical CoC + far-field + premultiplied alpha).
- Want "real" modern DoF without pain → **#3 (Jimenez/COD)**. It naturally reuses a bloom-style `ImageProcessTask` + downsample/blur setup.
- Want bokeh shapes → **#4**, accept the complexity.

---

## Part 2 — Jimenez/COD:AW separable gather DoF, in depth

### Reference
- **Jorge Jimenez, "Next Generation Post Processing in Call of Duty: Advanced Warfare," SIGGRAPH 2014** Advances in Real-Time Rendering course. Slides are the canonical source — the DoF section is ~40 slides with clear diagrams. (Same talk the 13-tap Karis bloom downsample is from.)
- Follow-ups: Sousa's "Graphics Gems from CryENGINE 3" (2013) for the separable-gather origin, and Abadie's "Advances in the Real-Time Rendering of Ghost Recon Wildlands" (GDC 2018) which uses a very similar pipeline.

### The core idea in one sentence
Do the whole DoF at **half resolution** in **two independent layers** — a *near* (foreground) layer and a *far* (background) layer — each produced by a **CoC-aware gather blur**, then composite them back over the sharp full-res image.

Why two layers? Because foreground and background bleeding have **opposite** correct behaviors:
- **Background blur** must NOT leak onto sharp foreground (a sharp silhouette against a blurry sky should stay crisp). → Solved by *rejecting* gather taps whose CoC is smaller than the center pixel's CoC.
- **Foreground blur** MUST leak onto sharp background (a blurry finger in front of a sharp face should feather outward over the face). → Solved by *premultiplying* color by CoC and letting it spread, plus a CoC dilation step.

Treating them as one layer is what causes every "simple DoF" to look wrong.

### The pipeline, step by step

#### Step 0 — CoC computation (thin lens)
```
A    = f / N                                       // aperture diameter
CoC  = A * (Z - S)/Z * f/(S - f)                   // world-space, signed
CoCp = CoC * (viewportHeightPx / sensorHeightM)    // convert to pixels
CoCp = clamp(CoCp, -maxCoC, maxCoC)                // bound tap count, e.g. ±24 px
```
Sign: `> 0` = background (far), `< 0` = foreground (near). Z must be linear eye-space depth.

`maxCoC` exists so the gather loop has a bounded tap radius. Pick something like 24 px at full res → 12 px at half res.

#### Step 1 — Downsample + CoC packing (half res)
One fragment shader, input = HDR color + depth, output = `RGBA16` where:
- `.rgb` = downsampled HDR color (use the **same 13-tap Karis kernel** as `bloomDownsample.frag` — the partial Karis average kills fireflies that would otherwise bloom through the gather blur as ugly bright dots)
- `.a` = signed CoC in pixels

Jimenez's trick: compute CoC per tap and take the **min of the 4 bilinear quad CoCs** (not the center CoC). This prevents a sharp pixel from "winning" a half-res texel and flickering between frames when the camera moves.

Then **split** this image into two half-res targets:
- `dofFar` — pass through pixels with `CoCp > 0`; set `CoCp <= 0` taps to CoC = 0 (they contribute sharp background to the gather and get rejected anyway).
- `dofNear` — for pixels with `CoCp < 0`, premultiply `.rgb *= saturate(-CoCp / maxCoC)` and store `saturate(-CoCp / maxCoC)` in `.a`. This is the key premultiplication that makes foreground spread correctly.

You can produce both targets in one pass with MRT, or run the same downsample shader twice with a push-constant flag. Two passes is simpler.

#### Step 2 — Near-field CoC dilation
Small 3×3 (or 5×5) **max** filter on `dofNear.a` (and carry `.rgb` along). Reason: a foreground object's out-of-focus silhouette should extend *outside* the object's original pixels. Without dilation, the near layer can only spread as far as the gather blur reaches from pixels that were already foreground; with dilation, you grow the "near" region first so the subsequent gather has something to spread from.

Far layer does NOT need dilation.

#### Step 3 — Separable gather blur (the workhorse)
Two passes — horizontal then vertical — each with ~13–17 taps along one axis. Run this separately for `dofNear` and `dofFar` (so four passes total, or use a push constant to flag axis + layer and schedule four task instances).

Tap offset for tap `i`:
```
offset_px = (i - kernelRadius) * (centerCoC / kernelRadius)
```
i.e. **tap spacing scales with the current pixel's CoC**. A sharp pixel (CoC ≈ 0) has offset ≈ 0 and samples itself; a max-CoC pixel spreads taps over the full kernel radius.

**Far layer weighting** (background gather):
```glsl
float w = step(centerCoC - epsilon, tapCoC);  // reject taps sharper than us
sumColor  += tapColor * w;
sumWeight += w;
```
This is the anti-bleeding rule: a blurry background pixel will not gather from a sharp foreground pixel, so sharp silhouettes stay sharp.

**Near layer weighting** (foreground gather):
```glsl
// tap.rgb is already premultiplied by tap CoC in step 1
sumColor  += tapColor;
sumWeight += tapAlpha;
```
Every near tap contributes, weighted by how "near" it is. Because of premultiplication + dilation, the foreground naturally spreads outward.

Final: `outColor.rgb = sumColor / max(sumWeight, epsilon)`; for the near layer, also write `outAlpha = sumWeight / tapCount` (or similar) — used for compositing.

A "separable" gather blur is technically an approximation for disc bokeh (a true disc isn't separable), but it's visually convincing and the quality difference is invisible in motion. To get rounder bokeh later, replace the 2-pass H/V with 3 skewed passes à la McIntosh — same framework, different offsets.

#### Step 4 — Composite (inside postProcess.frag)
At full res, with the full-res CoC:
```glsl
float cocFull = computeCocPixels(linearZ, ...);
vec3  sharp   = texture(sceneColor, uv).rgb;
vec4  farSmp  = texture(dofFar,  uv);           // bilinear upsample
vec4  nearSmp = texture(dofNear, uv);

float farW  = smoothstep(0.1, 1.0, max(cocFull, 0.0) / maxCoC);
float nearW = saturate(nearSmp.a * someScale);

vec3 color = mix(sharp, farSmp.rgb, farW);      // background blend
color      = mix(color, nearSmp.rgb, nearW);    // foreground on top
```
The `smoothstep(0.1, 1.0, ...)` hides the half-res→full-res boundary; pixels at tiny CoC stay fully sharp and avoid half-res resolution loss.

### Why it maps cleanly onto a bloom setup

A Jimenez bloom is already:
1. downsample (Karis 13-tap) →
2. multi-step blur →
3. composite in postProcess

DoF is:
1. downsample (same Karis 13-tap, different packing) →
2. dilate + separable blur →
3. composite in postProcess

Same `ImageProcessTask`, same shader layout, same RGBA16 half-res targets, same tiling, same clamp-edge sampler. The new pieces are:
- 3 new shaders (`dofDownsample.frag`, `dofNearCocDilate.frag`, `dofBlur.frag`)
- 2–3 new half-res RGBA16 images (`dofNear`, `dofFar`, optional temp ping-pong)
- Scheduling 6–7 task links after bloom upsample

You literally reuse the Karis downsample kernel twice in the same frame.

### Common pitfalls
- **Using non-linear depth for CoC** — do eye-space reconstruction; otherwise CoC flips sign at weird places.
- **Forgetting to premultiply the near layer** — foreground looks like it has a hard edge and doesn't feather.
- **Forgetting `step(centerCoC, tapCoC)` in the far gather** — sharp pixels bleed into blur, visible immediately against sky.
- **Max CoC too large** — flicker and undersampling. Start at 16–24 px and tune.
- **Compositing with raw CoC instead of smoothstepped weight** — visible half-res boundary at low CoC. The smoothstep threshold (~0.5–1 px CoC) hides it.
- **Reusing the bloom output image as a DoF source** — don't; bloom upsample mutates it in-place. Allocate dedicated half-res targets. Bloom and DoF should read the same `mainColorResolvedImage` independently.

### Tuning knobs once it works
- `maxCoCPixels`: the blur ceiling. Controls taste + tap cost.
- Kernel radius (tap count per axis): 7 / 13 / 17. 13 is the Jimenez default.
- Near dilation radius: 1 or 2 texels at half res.
- Aperture range in UI: f/1.4 → f/22 covers normal photography.
- Sensor size: 24 mm (full-frame) default; 16 mm feels like a phone camera, 36 mm feels cinematic.
