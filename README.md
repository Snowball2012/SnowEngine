# SnowEngine

Small DX12 render framework for educational purposes

Right now supports:
1) Multiple render passes
2) PBR Disney-like BRDF
3) Normal mapping
4) FBX loading
5) Shadow mapping with 3x3 gauss PCF
6) Simple static TXAA (8 halton(2,3) samples with 3x3 neighbourhood color clamping)
7) Simple tonemapping with adjustable photometric brightness bounds
8) Automatic graphics pipeline generation (compile-time resource bindings, runtime framegraph generation)
