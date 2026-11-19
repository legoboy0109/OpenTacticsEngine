// The Direct3D 11 backend's shaders (docs/RENDER3D.md "Direct3D 11 backend"). The only shaders the engine has on
// Windows; never in game data, never seen outside engine/gfx3d/d3d11.
//
// Compiled ahead of time to Shader Model 4.0 bytecode (feature level 10_0 and up, so also Xbox One's 10_1) by
// tools/windows/make_d3d11_shaders.ps1, which writes the *.h files next to this one. Those are committed: building the
// engine needs no shader compiler, and nothing compiles shaders at run time (d3dcompiler is not on every device).
//
// The arithmetic is the OpenGL backend's (gl/gl_renderer3d.cpp), which is the software backend's (soft3d.cpp): lighting
// and fog per vertex, the triangle only blends them. Differences from the GL shaders, all conventions of the API:
// - Clip-space depth: the engine's projections give -w..w (OpenGL); Direct3D wants 0..w, so z becomes (z + w) / 2.
//   Same depth precision, same near and far planes.
// - No upside-down drawing: a Direct3D render target's first row is its top, which is what the picture's first row
//   must be. (The GL backend negates y so that reading back gives the top row first.)

// ---- the 3D drawing ---------------------------------------------------------------------------------------------

cbuffer Draw : register(b0) {
  float4x4 uMvp;      // the engine's matrices as they are: column-major, used as mul(M, v)
  float4x4 uMv;
  float4x4 uModel;    // its upper-left 3 x 3 turns normals (the GL backend's uNormalM)
  float4 uColor;      // material colour
  float4 uToSun;      // xyz: towards the sun
  float4 uSun;        // rgb
  float4 uAmbient;    // rgb
  float4 uParams;     // blend mode (0 solid, 1 cut-out, 2 blended), cutoff, lit, unused
  float4 uFog;        // start, 1 / (end - start), on, unused
  float4 uFogColor;   // rgb, on
};

Texture2D uTex : register(t0);
SamplerState uSampler : register(s0);

struct VsIn {
  float3 pos : POSITION;
  float3 normal : NORMAL;
  float2 uv : TEXCOORD0;
  float4 color : COLOR0;  // R8G8B8A8_UNORM
};

struct VsOut {
  float4 pos : SV_Position;
  float4 color : COLOR0;
  // Centroid: with multisampling the position must stay inside the triangle, or a tile's edge samples read the
  // transparent padding of its sheet (dark seams). Same as the GL backend.
  centroid float2 uv : TEXCOORD0;
  float fog : TEXCOORD1;
};

VsOut vsMain(VsIn i) {
  VsOut o;
  float3 light = float3(1.0, 1.0, 1.0);
  if (uParams.z > 0.5) {
    float3 n = mul((float3x3)uModel, i.normal);
    float len = length(n);
    if (len > 0.0) n /= len;
    light = uAmbient.rgb + uSun.rgb * max(0.0, dot(n, uToSun.xyz));
  }
  o.color = saturate(float4(i.color.rgb * uColor.rgb * light, i.color.a * uColor.a));
  o.uv = i.uv;
  o.fog = 0.0;
  if (uFog.z > 0.5) o.fog = saturate((length(mul(uMv, float4(i.pos, 1.0)).xyz) - uFog.x) * uFog.y);
  float4 p = mul(uMvp, float4(i.pos, 1.0));
  p.z = (p.z + p.w) * 0.5;
  o.pos = p;
  return o;
}

float4 psMain(VsOut i) : SV_Target {
  float4 c = i.color * uTex.Sample(uSampler, i.uv);
  if (uParams.x > 0.5 && uParams.x < 1.5 && c.a < uParams.y) discard;
  if (uFogColor.a > 0.5) c.rgb = lerp(c.rgb, uFogColor.rgb, i.fog);
  return float4(c.rgb, uParams.x > 1.5 ? c.a : 1.0);
}

// ---- putting the finished picture on the screen -------------------------------------------------------------------
// One quad (a 4-vertex strip from SV_VertexID, no vertex buffer) over the frame's rectangle, sampling the picture the
// way SDL draws a texture (Show3D::show does for the other backends): plain texture colour, blended by the pipeline.

cbuffer Quad : register(b0) {
  float4 uRect;  // left, top, right, bottom in clip space
};

struct QuadOut {
  float4 pos : SV_Position;
  float2 uv : TEXCOORD0;
};

QuadOut quadVsMain(uint id : SV_VertexID) {
  float2 t = float2((float)(id & 1u), (float)(id >> 1));  // (0,0) (1,0) (0,1) (1,1)
  QuadOut o;
  o.pos = float4(lerp(uRect.x, uRect.z, t.x), lerp(uRect.y, uRect.w, t.y), 0.0, 1.0);
  o.uv = t;
  return o;
}

float4 quadPsMain(QuadOut i) : SV_Target { return uTex.Sample(uSampler, i.uv); }
