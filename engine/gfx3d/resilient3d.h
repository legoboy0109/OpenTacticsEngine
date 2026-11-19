// A hardware 3D backend that survives losing its device (docs/RENDER3D.md "Losing the device").
//
// A GPU context or device can go away under a running game: Android may take an EGL context when the app goes to the
// background, Direct3D reports DXGI_ERROR_DEVICE_REMOVED / RESET after a driver update, a GPU reset or a device
// unplugged, and SDL's own renderer then makes a new device. A backend's meshes and textures go with it.
//
// ResilientRenderer3D wraps the backend: it keeps a CPU copy of every mesh and texture the game made and, when the
// platform says the device is gone, asks the platform for a new backend and replays them, so the game's ids stay valid.
// If no new backend can be made, the same copies are replayed into the software backend for the rest of the session.
// Nothing above notices, apart from the log lines. No graphics API in here: what "lost" means and how a new backend
// is made is the platform's DeviceKeeper.
//
// Debug (the platforms read these from the environment and pass them in): OTE_3D_SIMULATE_LOSS=N makes the device be
// lost after N frames (DeviceKeeper::simulateLoss), to test the above; with OTE_3D_SIMULATE_NO_REBUILD=1 as well the
// rebuild is refused, which tests the software fallback.
#pragma once
#include <memory>
#include <string>
#include <vector>

#include "gfx3d/render3d.h"

namespace ote {

// What the platform provides for one kind of device (EGL context, Direct3D 11 device).
class DeviceKeeper {
 public:
  virtual ~DeviceKeeper() {}
  // Prefix of the log lines, e.g. "3D (EGL)".
  virtual const char* name() const = 0;
  // What a loss is on this platform, for the log ("the 3D context was lost (backgrounded, GPU reset or ...)").
  virtual const char* lossText() const = 0;
  // True when the current backend's device is gone and a new one must be made.
  virtual bool lost() = 0;
  // A new backend on a new (or recovered) device; null with the reason in `why`.
  virtual std::unique_ptr<Renderer3D> rebuild(std::string& why) = 0;
  // Debug: make the current device be lost, as a real loss would (the next use finds it gone).
  virtual void simulateLoss() = 0;
  // The device is no longer needed (the software backend took over, or the wrapper goes away). Called after the
  // backend that used it was destroyed.
  virtual void release() = 0;
};

class ResilientRenderer3D : public Renderer3D {
 public:
  // `simulateLossAt` > 0: the device is lost after that many frames; `refuseRebuild`: rebuilding fails (debug).
  ResilientRenderer3D(std::unique_ptr<Renderer3D> inner, std::unique_ptr<DeviceKeeper> keeper, int simulateLossAt = 0,
                      bool refuseRebuild = false);
  ~ResilientRenderer3D() override;

  Caps3D caps() const override { return inner_->caps(); }
  MeshId createMesh(const MeshData& data) override;
  void updateMesh(MeshId mesh, const MeshData& data) override;
  void destroyMesh(MeshId mesh) override;
  Texture3DId createTexture(const uint8_t* rgba, int width, int height, bool repeat, bool mipmaps = false) override;
  void destroyTexture(Texture3DId texture) override;
  void begin(const Frame3D& frame) override;
  void draw(MeshId mesh, const Mat4& model, const Material3D& material) override;
  void end() override;
  const uint8_t* framePixels(int& width, int& height) const override { return inner_->framePixels(width, height); }

  // For tests: true once the software backend has taken over.
  bool usingSoftware() const { return software_; }

 private:
  struct Mesh {
    bool used = false;
    MeshData data;
    MeshId inner = 0;
  };
  struct Texture {
    bool used = false, repeat = false, mipmaps = false;
    std::vector<uint8_t> rgba;
    int w = 0, h = 0;
    Texture3DId inner = 0;
  };
  struct Draw {
    MeshId mesh;
    Mat4 model;
    Material3D material;
  };

  bool validMesh(MeshId m) const { return m >= 1 && (size_t)m <= meshes_.size() && meshes_[(size_t)m - 1].used; }
  Material3D innerMaterial(Material3D m) const;
  bool recoverIfLost();  // true when something was rebuilt
  void replay();
  size_t countMeshes() const;
  size_t countTextures() const;

  std::unique_ptr<Renderer3D> inner_;
  std::unique_ptr<DeviceKeeper> keeper_;
  std::vector<Mesh> meshes_;
  std::vector<Texture> textures_;
  Frame3D frame_;
  std::vector<Draw> draws_;
  bool inFrame_ = false, software_ = false;
  int simulateAt_ = 0, frames_ = 0;
  bool refuseRebuild_ = false;
};

}  // namespace ote
