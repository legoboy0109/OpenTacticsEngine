!!VP1.1
# The one vertex program of the original Xbox's 3D backend (docs/RENDER3D.md "Original Xbox backend"). Source for
# tools/xbox-og/shaders/make_shaders.sh (nxdk's vp20compiler); the generated engine/platform/nxdk/hw3d_vp.inl is committed.
# It does what the software backend does per vertex: position, lighting from one sun plus ambient, vertex colour times
# material colour, and fog. Every instruction reads at most one constant register and one input (a hardware rule).
#
# inputs:  v[0] position, v[2] normal, v[3] colour (4 unsigned bytes, 0..1), v[8] texture position (hardware attribute 9)
# constants (program numbers; the program is loaded with constant 0 at hardware slot 96):
#   c[0..3]  rows of projection * view * model              c[4] window scale  (w/2, -h/2, zscale/2, 1)
#   c[5]     window offset (x + w/2, y + h/2, zscale/2, 0)    c[6..8] rows of the model's rotation part (for the normal)
#   c[9]     direction towards the sun                       c[10] ambient colour  c[11] sun colour (a unit lights nothing: 0)
#   c[12]    material colour                                 c[13..15] rows of view * model (for the distance)
#   c[16]    (1 / (fogEnd - fogStart), -fogStart / (fogEnd - fogStart), 0, 0)    c[17] fog colour
#   c[18] (0, 1, 0, 0) and c[19] (1e-8, 0, 0, 0): kept by the program
# outputs: o[HPOS] window position (divided by w by hand, the hardware wants it so), o[COL0] colour, lit and shrunk by
#          (1 - fog), o[COL1].xyz fog amount * fog colour (the pixel stage adds it after the texture: texture * colour *
#          (1 - fog) + fog * fog colour is the software backend's blend), o[TEX0] texture position

# position
DP4 R0.x, v[0], c[0];
DP4 R0.y, v[0], c[1];
DP4 R0.z, v[0], c[2];
DP4 R0.w, v[0], c[3];
RCC R1.x, R0.w;
MUL R0.xyz, R0, R1.x;
MOV R1, c[4];
MUL R0.xyz, R0, R1;
MOV R1, c[5];
ADD o[HPOS].xyz, R0, R1;
MOV o[HPOS].w, R0.w;

# fog amount (0 none .. 1 all fog) from the distance to the camera
MOV R4, c[18];
MOV R10, c[19];
DP4 R7.x, v[0], c[13];
DP4 R7.y, v[0], c[14];
DP4 R7.z, v[0], c[15];
DP3 R7.w, R7, R7;
MAX R7.w, R7.w, R10.x;
RSQ R8.x, R7.w;
MUL R7.w, R7.w, R8.x;
MOV R9, c[16];
MAD R7.w, R7.w, R9.x, R9.y;
MAX R7.w, R7.w, R4.x;
MIN R7.w, R7.w, R4.y;
MOV R1, c[17];
MUL o[COL1].xyz, R1, R7.w;
ADD R7.z, R4.y, -R7.w;

# lighting: normal turned by the model, made unit length
DP3 R2.x, v[2], c[6];
DP3 R2.y, v[2], c[7];
DP3 R2.z, v[2], c[8];
DP3 R2.w, R2, R2;
MAX R2.w, R2.w, R10.x;
RSQ R2.w, R2.w;
MUL R2.xyz, R2, R2.w;
DP3 R3.x, R2, c[9];
MAX R3.x, R3.x, R4.x;
MOV R5, c[11];
MUL R5, R5, R3.x;
MOV R6, c[10];
ADD R5, R5, R6;
MOV R6, c[12];
MUL R6, R6, v[3];
MUL R5, R6, R5;
MIN R5, R5, R4.yyyy;
MUL o[COL0].xyz, R5, R7.zzzz;
MOV o[COL0].w, R6.w;

MOV o[TEX0], v[8];
END
