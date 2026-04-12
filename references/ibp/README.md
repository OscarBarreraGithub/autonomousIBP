# IBP Backends

This section covers the reduction engines AMFlow currently interfaces with or that are most relevant to a replacement strategy.

- `kira/`
  Primary backend candidate. Best fit for a C++ AMFlow rewrite.
- `fire/`
  Important for simplifier architecture, modular arithmetic, and historical comparison.
- `litered/`
  Mathematica-based reference for sector analysis, DEs, and DRRs.
- `blade/`
  Same-group reduction ideas, especially block-triangular structure.

If only one backend stack is read deeply, it should be `Kira 3 + Kira 2 + Kira README/docs`.
