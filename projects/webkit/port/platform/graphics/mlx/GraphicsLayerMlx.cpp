// The Mlx port's GraphicsLayer: GraphicsLayer::create, which every port
// defines with its compositing (CoreAnimation, TextureMapper, coordinated
// graphics). The port paints a page whole into a Skia raster surface
// (PORT.md, "Graphics"), with accelerated compositing off, so WebCore
// makes no layer of its own; a factory the embedding gives makes its own.
//
// STUB (M8: composited layers over Skia on the CPU): a layer made without
// a factory keeps its geometry (GraphicsLayer's) and paints nothing.
#include "config.h"
#include "GraphicsLayer.h"

#include "GraphicsLayerFactory.h"

namespace WebCore {

class GraphicsLayerMlx final : public GraphicsLayer {
public:
    GraphicsLayerMlx(Type layerType, GraphicsLayerClient& client)
        : GraphicsLayer(layerType, client)
    {
    }

private:
    void setNeedsDisplay() final { }
    void setNeedsDisplayInRect(const FloatRect&, ShouldClipToLayer) final { }
};

Ref<GraphicsLayer> GraphicsLayer::create(GraphicsLayerFactory* factory, GraphicsLayerClient& client, Type layerType)
{
    if (factory)
        return factory->createGraphicsLayer(layerType, client);
    return adoptRef(*new GraphicsLayerMlx(layerType, client));
}

} // namespace WebCore
