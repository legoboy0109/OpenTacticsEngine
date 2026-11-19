#ifdef _WIN32
#include "gfx3d/d3d11/d3d11_device.h"

#include <dxgi.h>

#include <cstdio>
#include <vector>

namespace ote {

OwnD3D11Host::OwnD3D11Host(Microsoft::WRL::ComPtr<ID3D11Device> dev, std::shared_ptr<D3D11LossFlag> loss)
    : dev_(dev), loss_(loss) {
  if (dev_) dev_->GetImmediateContext(ctx_.GetAddressOf());
}

bool OwnD3D11Host::enter() {
  if (!dev_ || !ctx_ || loss_->shutDown || loss_->simulated) return false;
  return dev_->GetDeviceRemovedReason() == S_OK;
}

Microsoft::WRL::ComPtr<ID3D11Device> makeD3D11Device(const D3D11DeviceChoice& choice, std::string& why) {
  // From the best down to 10_0, the least the backend needs; the first `skip` are left out to cap the level.
  static const D3D_FEATURE_LEVEL all[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1,
                                          D3D_FEATURE_LEVEL_10_0};
  int skip = 0;
  if (choice.maxLevel == "11_0") skip = 1;
  else if (choice.maxLevel == "10_1") skip = 2;
  else if (choice.maxLevel == "10_0") skip = 3;
  else if (!choice.maxLevel.empty() && choice.maxLevel != "11_1") {
    why = "unknown feature level '" + choice.maxLevel + "' (11_1, 11_0, 10_1 or 10_0)";
    return nullptr;
  }
  const D3D_DRIVER_TYPE type = choice.warp ? D3D_DRIVER_TYPE_WARP : D3D_DRIVER_TYPE_HARDWARE;
  Microsoft::WRL::ComPtr<ID3D11Device> dev;
  D3D_FEATURE_LEVEL got;
  HRESULT hr = D3D11CreateDevice(nullptr, type, nullptr, 0, all + skip, (UINT)(4 - skip), D3D11_SDK_VERSION,
                                 dev.GetAddressOf(), &got, nullptr);
  if (hr == E_INVALIDARG && skip == 0)  // Windows 7 without its platform update does not know 11_1
    hr = D3D11CreateDevice(nullptr, type, nullptr, 0, all + 1, 3, D3D11_SDK_VERSION, dev.GetAddressOf(), &got, nullptr);
  if (FAILED(hr)) {
    char buf[160];
    snprintf(buf, sizeof(buf), "no Direct3D 11 device of feature level 10_0 or better (%s, HRESULT 0x%08lx)",
             choice.warp ? "WARP" : "hardware", (unsigned long)hr);
    why = buf;
    return nullptr;
  }
  return dev;
}

bool isSoftwareD3D11Device(ID3D11Device* dev) {
  Microsoft::WRL::ComPtr<IDXGIDevice> dxgi;
  Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
  DXGI_ADAPTER_DESC desc;
  if (!dev || FAILED(dev->QueryInterface(__uuidof(IDXGIDevice), (void**)dxgi.GetAddressOf())) ||
      FAILED(dxgi->GetAdapter(adapter.GetAddressOf())) || FAILED(adapter->GetDesc(&desc)))
    return false;
  return desc.VendorId == 0x1414 && desc.DeviceId == 0x8c;  // Microsoft, Basic Render Driver (WARP)
}

}  // namespace ote

#endif  // _WIN32
