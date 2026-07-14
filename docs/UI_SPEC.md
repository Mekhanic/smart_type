# SmartType Candidate Panel UI Specification

> [!IMPORTANT]
> **Current baseline**: These values describe the installed candidate-panel design. They may be changed by a deliberate visual task; functional fixes should avoid unrelated visual changes.

## 1. Candidate Panel Dimensions
- **Content invariant**: the panel renders candidates and optional auxiliary
  status only. Ordinary composition text belongs to the target application's
  field and must not appear as a separate editor row in this window.
- **Panel Height**: 34 logical px
- **Corner Radius**: 12 px
- **Horizontal Gap between Candidates**: 4 px
- **Pill Inset**: 3 px

## 2. Spacing & Margins
- **Candidate Horizontal Padding**: 8 px
- **Candidate Vertical Padding**: 4 px

## 3. Typography
- **Font Family**: Noto Sans Medium
- **Font Size**: 10 pt
- **Case Rules**: Preserve the case pattern of the typed preedit string.

## 4. Animation Settings
- **Pill Interpolation**: Move selection pill to target index candidate capsule.
- **Duration**: 140 ms
- **Curve**: `easeOutCubic`
- **Correction Flash/Pulse Overlay**: 120 ms horizontal sweep overlay.
