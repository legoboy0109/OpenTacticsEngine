#ifdef _WIN32
#include "gfx3d/d3d11/d3d11_state.h"

namespace ote {

namespace {
template <class T>
void drop(T*& p) {
  if (p) p->Release();
  p = nullptr;
}
}  // namespace

// Slot 0 of everything is all SDL (2 and 3) and the 3D backend use; the backend binds nothing beyond slot 0 and one
// render target.
void D3D11StateBackup::save(ID3D11DeviceContext* c) {
  clear();
  c->IAGetInputLayout(&layout_);
  c->IAGetPrimitiveTopology(&topology_);
  c->IAGetVertexBuffers(0, 1, &vb_, &vbStride_, &vbOffset_);
  c->IAGetIndexBuffer(&ib_, &ibFormat_, &ibOffset_);
  c->VSGetShader(&vs_, nullptr, nullptr);
  c->PSGetShader(&ps_, nullptr, nullptr);
  c->GSGetShader(&gs_, nullptr, nullptr);
  c->VSGetConstantBuffers(0, 1, &vsCb_);
  c->PSGetConstantBuffers(0, 1, &psCb_);
  c->PSGetShaderResources(0, 1, &psSrv_);
  c->PSGetSamplers(0, 1, &psSampler_);
  c->RSGetState(&raster_);
  viewportCount_ = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
  c->RSGetViewports(&viewportCount_, viewports_);
  scissorCount_ = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
  c->RSGetScissorRects(&scissorCount_, scissors_);
  c->OMGetBlendState(&blend_, blendFactor_, &sampleMask_);
  c->OMGetDepthStencilState(&depth_, &stencilRef_);
  c->OMGetRenderTargets(1, &rtv_, &dsv_);
  saved_ = true;
}

void D3D11StateBackup::restore(ID3D11DeviceContext* c) {
  if (!saved_) return;
  // Unbind the backend's shader resource first: SDL's own render target texture must never be bound as input and
  // output at once (the runtime would silently unbind one).
  ID3D11ShaderResourceView* none = nullptr;
  c->PSSetShaderResources(0, 1, &none);
  c->OMSetRenderTargets(1, &rtv_, dsv_);
  c->IASetInputLayout(layout_);
  c->IASetPrimitiveTopology(topology_);
  c->IASetVertexBuffers(0, 1, &vb_, &vbStride_, &vbOffset_);
  c->IASetIndexBuffer(ib_, ibFormat_, ibOffset_);
  c->VSSetShader(vs_, nullptr, 0);
  c->PSSetShader(ps_, nullptr, 0);
  c->GSSetShader(gs_, nullptr, 0);
  c->VSSetConstantBuffers(0, 1, &vsCb_);
  c->PSSetConstantBuffers(0, 1, &psCb_);
  c->PSSetShaderResources(0, 1, &psSrv_);
  c->PSSetSamplers(0, 1, &psSampler_);
  c->RSSetState(raster_);
  c->RSSetViewports(viewportCount_, viewports_);
  c->RSSetScissorRects(scissorCount_, scissors_);
  c->OMSetBlendState(blend_, blendFactor_, sampleMask_);
  c->OMSetDepthStencilState(depth_, stencilRef_);
  clear();
}

void D3D11StateBackup::clear() {
  drop(layout_);
  drop(vb_);
  drop(ib_);
  drop(vs_);
  drop(ps_);
  drop(gs_);
  drop(vsCb_);
  drop(psCb_);
  drop(psSrv_);
  drop(psSampler_);
  drop(raster_);
  drop(blend_);
  drop(depth_);
  drop(rtv_);
  drop(dsv_);
  viewportCount_ = scissorCount_ = 0;
  saved_ = false;
}

}  // namespace ote

#endif  // _WIN32
