#!/bin/bash

echo "Compiling HLSL shaders..."

# Existing shaders
dxc -T vs_6_0 -E vsmain -Fo bin/vertex.cso     vertex.hlsl
dxc -T ps_6_0 -E psmain -Fo bin/pixel.cso      pixel.hlsl
dxc -T vs_6_0 -E vsmain -Fo bin/grid_vs.cso    grid_vs.hlsl
dxc -T ps_6_0 -E psmain -Fo bin/grid_ps.cso    grid_ps.hlsl

# Terrain shaders
dxc -T vs_6_0 -E vsmain   -Fo bin/terrain_vs.cso  terrain_vs.hlsl
dxc -T hs_6_0 -E hsmain   -Fo bin/terrain_hs.cso  terrain_hs.hlsl
dxc -T ds_6_0 -E dsmain   -Fo bin/terrain_ds.cso  terrain_ds.hlsl
dxc -T ps_6_0 -E psmain   -Fo bin/terrain_ps.cso  terrain_ps.hlsl

# Sky shaders
dxc -T vs_6_0 -E vsmain -Fo bin/sky_vs.cso sky_vs.hlsl
dxc -T ps_6_0 -E psmain -Fo bin/sky_ps.cso sky_ps.hlsl

# Particle shaders
dxc -T cs_6_0 -E csmain -Fo bin/particles_cs.cso particles_cs.hlsl
dxc -T vs_6_0 -E vsmain -Fo bin/particles_vs.cso particles_vs.hlsl
dxc -T ps_6_0 -E psmain -Fo bin/particles_ps.cso particles_ps.hlsl

echo "Shader compilation done."