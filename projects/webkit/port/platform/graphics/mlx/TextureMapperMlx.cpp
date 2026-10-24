// The Mlx port's TextureMapper and TextureMapperGPUBuffer: texmap's GL
// renderer of layers (TextureMapper.cpp, TextureMapperGPUBuffer.cpp: GL,
// not compiled). Coordinated graphics keep a TextureMapper path beside the
// Skia one in every layer (CoordinatedPlatformLayer's TextureMapperLayer
// target, texmap's backing stores), and WebKit's sources name these
// functions from it; the port composites with Skia on the CPU
// (MlxLayerTreeHost: SkiaCompositingLayer), so nothing reaches them.
//
// STUB (M8: the TextureMapper path of coordinated graphics, which the
// port's compositor never takes): each function is a crash if reached.
#include "config.h"
#include "TextureMapper.h"

#include "BitmapTexture.h"
#include "ClipPath.h"
#include "FilterOperations.h"
#include "TextureMapperGPUBuffer.h"

namespace WebCore {

void TextureMapper::drawBorder(const Color&, float, const FloatRect&, const TransformationMatrix&) { RELEASE_ASSERT_NOT_REACHED(); }
void TextureMapper::drawNumber(int, const Color&, const FloatPoint&, const TransformationMatrix&) { RELEASE_ASSERT_NOT_REACHED(); }
void TextureMapper::drawTexture(const BitmapTexture&, const FloatRect&, const TransformationMatrix&, float, AllEdgesExposed) { RELEASE_ASSERT_NOT_REACHED(); }
void TextureMapper::drawTextureFragment(const BitmapTexture&, const FloatRect&, const FloatRect&) { RELEASE_ASSERT_NOT_REACHED(); }
void TextureMapper::drawSolidColor(const FloatRect&, const TransformationMatrix&, const Color&, bool) { RELEASE_ASSERT_NOT_REACHED(); }
void TextureMapper::bindSurface(BitmapTexture*) { RELEASE_ASSERT_NOT_REACHED(); }
BitmapTexture* TextureMapper::currentSurface() { RELEASE_ASSERT_NOT_REACHED(); }
void TextureMapper::beginClip(const TransformationMatrix&, const FloatRoundedRect&) { RELEASE_ASSERT_NOT_REACHED(); }
void TextureMapper::beginClip(const TransformationMatrix&, const ClipPath&) { RELEASE_ASSERT_NOT_REACHED(); }
void TextureMapper::beginClipWithoutApplying(const TransformationMatrix&, const FloatRect&) { RELEASE_ASSERT_NOT_REACHED(); }
void TextureMapper::endClip() { RELEASE_ASSERT_NOT_REACHED(); }
void TextureMapper::endClipWithoutApplying() { RELEASE_ASSERT_NOT_REACHED(); }
IntRect TextureMapper::clipBounds() { RELEASE_ASSERT_NOT_REACHED(); }
IntSize TextureMapper::maxTextureSize() const { RELEASE_ASSERT_NOT_REACHED(); }
void TextureMapper::setDepthRange(double, double) { RELEASE_ASSERT_NOT_REACHED(); }
std::pair<double, double> TextureMapper::depthRange() const { RELEASE_ASSERT_NOT_REACHED(); }
RefPtr<BitmapTexture> TextureMapper::applyFilters(RefPtr<BitmapTexture>&, const FilterOperations&, bool) { RELEASE_ASSERT_NOT_REACHED(); }
Ref<TextureMapperGPUBuffer> TextureMapper::acquireBufferFromPool(size_t, TextureMapperGPUBuffer::Type) { RELEASE_ASSERT_NOT_REACHED(); }

TextureMapperGPUBuffer::~TextureMapperGPUBuffer() = default;
bool TextureMapperGPUBuffer::updateData(const void*, size_t, size_t) { RELEASE_ASSERT_NOT_REACHED(); }

} // namespace WebCore
