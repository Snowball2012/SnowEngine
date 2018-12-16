# SnowEngine

Small DX12 render framework for educational purposes

Right now supports:
1) PBR direct lighting (Disney diffuse + GGX specular)
2) Normal mapping
3) Shadow mapping with 3x3 gauss PCF
4) HBAO
5) Simple tonemapping with adjustable photometric brightness bounds
6) FBX loading
7) Multiple render passes
8) Automatic graphics pipeline generation (compile-time resource bindings, runtime framegraph generation)
9) Partial mip residency (via dx12 reserved resources), per-frame evaluation with static budget
10) Renderer-oriented scene graph
