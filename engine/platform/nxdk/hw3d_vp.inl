// The one vertex program of the original Xbox's 3D backend (docs/RENDER3D.md "Original Xbox backend"). Source for
// tools/xbox-og/shaders/make_shaders.sh (nxdk's vp20compiler); the generated engine/platform/nxdk/hw3d_vp.inl is committed.
// It does what the software backend does per vertex: position, lighting from one sun plus ambient, vertex colour times
// material colour, and fog. Every instruction reads at most one constant register and one input (a hardware rule).
//
// inputs:  v[0] position, v[2] normal, v[3] colour (4 unsigned bytes, 0..1), v[8] texture position (hardware attribute 9)
// constants (program numbers; the program is loaded with constant 0 at hardware slot 96):
//   c[0..3]  rows of projection * view * model              c[4] window scale  (w/2, -h/2, zscale/2, 1)
//   c[5]     window offset (x + w/2, y + h/2, zscale/2, 0)    c[6..8] rows of the model's rotation part (for the normal)
//   c[9]     direction towards the sun                       c[10] ambient colour  c[11] sun colour (a unit lights nothing: 0)
//   c[12]    material colour                                 c[13..15] rows of view * model (for the distance)
//   c[16]    (1 / (fogEnd - fogStart), -fogStart / (fogEnd - fogStart), 0, 0)    c[17] fog colour
//   c[18] (0, 1, 0, 0) and c[19] (1e-8, 0, 0, 0): kept by the program
// outputs: o[HPOS] window position (divided by w by hand, the hardware wants it so), o[COL0] colour, lit and shrunk by
//          (1 - fog), o[COL1].xyz fog amount * fog colour (the pixel stage adds it after the texture: texture * colour *
//          (1 - fog) + fog * fog colour is the software backend's blend), o[TEX0] texture position
0x00000000, 0x00ec001b, 0x0836186c, 0x280007f8,
0x00000000, 0x00ec201b, 0x0836186c, 0x240007f8,
0x00000000, 0x00ec401b, 0x0836186c, 0x220007f8,
0x00000000, 0x00ec601b, 0x0836186c, 0x210007f8,
0x00000000, 0x0600001b, 0x08361300, 0x101807f8,
0x00000000, 0x0040001b, 0x0400286c, 0x2e0007f8,
0x00000000, 0x002c801b, 0x0c36106c, 0x2f1007f8,
0x00000000, 0x0040001b, 0x0436286c, 0x2e0007f8,
0x00000000, 0x002ca01b, 0x0c36106c, 0x2f1007f8,
0x00000000, 0x0060001b, 0x0436106c, 0x5070e800,
0x00000000, 0x002000ff, 0x0436106c, 0x20701800,
0x00000000, 0x002e401b, 0x0c36106c, 0x2f4007f8,
0x00000000, 0x002e601b, 0x0c36106c, 0x2fa007f8,
0x00000000, 0x00eda01b, 0x0836186c, 0x287007f8,
0x00000000, 0x00edc01b, 0x0836186c, 0x247007f8,
0x00000000, 0x00ede01b, 0x0836186c, 0x227007f8,
0x00000000, 0x00a0001b, 0x7436e86c, 0x217007f8,
0x00000000, 0x014000ff, 0x7401486c, 0x217007f8,
0x00000000, 0x0800001b, 0x08361301, 0xd08807f8,
0x00000000, 0x004000ff, 0x7401086c, 0x217007f8,
0x00000000, 0x002e001b, 0x0c36106c, 0x2f9007f8,
0x00000000, 0x008000ff, 0x74012956, 0x517007f8,
0x00000000, 0x014000ff, 0x7400886c, 0x217007f8,
0x00000000, 0x012000ff, 0x74aa886c, 0x217007f8,
0x00000000, 0x002e201b, 0x0c36106c, 0x2f1007f8,
0x00000000, 0x0040001b, 0x15fee86c, 0x2070e820,
0x00000000, 0x00600055, 0x443617fd, 0xd27007f8,
0x00000000, 0x00acc41b, 0x0836186c, 0x282007f8,
0x00000000, 0x00ace41b, 0x0836186c, 0x242007f8,
0x00000000, 0x00ad041b, 0x0836186c, 0x222007f8,
0x00000000, 0x00a0001b, 0x2436486c, 0x212007f8,
0x00000000, 0x014000ff, 0x2401486c, 0x212007f8,
0x00000000, 0x0800001b, 0x08361300, 0x902107f8,
0x00000000, 0x0040001b, 0x25fe486c, 0x2e2007f8,
0x00000000, 0x00ad201b, 0x2436186c, 0x283007f8,
0x00000000, 0x01400000, 0x3400886c, 0x283007f8,
0x00000000, 0x002d601b, 0x0c36106c, 0x2f5007f8,
0x00000000, 0x0040001b, 0x5400686c, 0x2f5007f8,
0x00000000, 0x002d401b, 0x0c36106c, 0x2f6007f8,
0x00000000, 0x0060001b, 0x5436106d, 0x9f5007f8,
0x00000000, 0x002d801b, 0x0c36106c, 0x2f6007f8,
0x00000000, 0x0040061b, 0x6436106c, 0x2f6007f8,
0x00000000, 0x0040001b, 0x6436a86c, 0x2f5007f8,
0x00000000, 0x0120001b, 0x54aa886c, 0x2f5007f8,
0x00000000, 0x0040001b, 0x5554e86c, 0x2070e818,
0x00000000, 0x002000ff, 0x6436106c, 0x20701818,
0x00000000, 0x0020121b, 0x0836106c, 0x2070f849,
