# SnowEngine

Small DX12 render framework for educational purposes

![](/screenshots/RT_Shadows_TAA.png)

Right now supports:
1) PBR direct lighting (Disney diffuse + GGX specular)
2) Normal mapping
3) Shadow mapping with 3x3 gauss PCF
4) RT direct sun shadows (WIP)
5) HBAO
6) Simple tonemapping with adjustable photometric brightness bounds
7) TAA
8) FBX loading
9) Automatic graphics pipeline generation (compile-time resource bindings, runtime framegraph generation)
10) Partial mip residency (via dx12 reserved resources), per-frame evaluation with static budget
