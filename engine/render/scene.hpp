#pragma once
#include "assets/package.hpp"
#include "render/context.hpp"
#include <memory>
#include <optional>
#include <stop_token>
#include <vector>

namespace ngen::render {
class SceneUpload;
// Immutable after complete upload. GPU values are accessed/released on the owner
// thread; worker jobs retain the owning CPU package pin, not native handles.
class SceneVersion {
  public:
    [[nodiscard]] assets::PackagePin package() const { return package_; }
    [[nodiscard]] const Buffer &vertices() const { return vertices_; }
    [[nodiscard]] const Buffer &indices() const { return indices_; }
    [[nodiscard]] const std::vector<Image> &images() const { return images_; }

  private:
    friend class SceneUpload;
    assets::PackagePin package_;
    Buffer vertices_;
    Buffer indices_;
    std::vector<Image> images_;
};
using ScenePin = std::shared_ptr<const SceneVersion>;
class SceneUpload {
  public:
    // Input is an owning immutable result of the checked package loader.
    [[nodiscard]] static std::variant<std::unique_ptr<SceneUpload>, Failure>
    begin(Context &context, assets::PackagePin package);
    ~SceneUpload();
    SceneUpload(const SceneUpload &) = delete;
    SceneUpload &operator=(const SceneUpload &) = delete;
    // Empty means bounded preparation is still pending; no partial version escapes.
    [[nodiscard]] std::variant<std::optional<ScenePin>, Failure>
    advance(Context &context, const std::stop_token &stop = {});

  private:
    SceneUpload();
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace ngen::render
