#include "InvertYRenderer.hpp"

#include <Gfx/Graph/RenderList.hpp>

namespace Gfx
{

InvertYRenderer::InvertYRenderer(
    const score::gfx::Node& n, score::gfx::TextureRenderTarget rt,
    QRhiReadbackResult& readback)
    : score::gfx::OutputNodeRenderer{n}
    , m_inputTarget{std::move(rt)}
    , m_readback{&readback}
{
}

void InvertYRenderer::init(
    score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  m_renderTarget = score::gfx::createRenderTarget(
      renderer.state, QRhiTexture::Format::RGBA8, m_inputTarget.texture->pixelSize(),
      renderer.samples(), renderer.requiresDepth());

  const auto& mesh = renderer.defaultTriangle();
  m_mesh = renderer.initMeshBuffer(mesh, res);

  // We need to have a pass to invert the Y coordinate to go
  // from GL direction (Y up) to normal video (Y down)
  // FIXME spout likely needs the same
  static const constexpr auto gl_filter = R"_(#version 450
    layout(location = 0) in vec2 v_texcoord;
    layout(location = 0) out vec4 fragColor;

    layout(binding = 3) uniform sampler2D tex;

    void main()
    {
      fragColor = texture(tex, vec2(v_texcoord.x, 1. - v_texcoord.y));
    }
    )_";
  std::tie(m_vertexS, m_fragmentS)
      = score::gfx::makeShaders(renderer.state, mesh.defaultVertexShader(), gl_filter);

  // Put the input texture, where all the input nodes are rendering, in a sampler.
  {
    auto sampler = renderer.state.rhi->newSampler(
        QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
        QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);

    sampler->setName("FullScreenImageNode::sampler");
    sampler->create();

    m_samplers.push_back({sampler, this->m_inputTarget.texture});
  }

  m_p = score::gfx::buildPipeline(
      renderer, mesh, m_vertexS, m_fragmentS, m_renderTarget, nullptr, nullptr,
      m_samplers);
}

void InvertYRenderer::update(
    score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res, score::gfx::Edge* e)
{
}

void InvertYRenderer::release(score::gfx::RenderList&)
{
  m_p.release();
  for(auto& s : m_samplers)
  {
    delete s.sampler;
  }
  m_samplers.clear();
  m_renderTarget.release();
}

void InvertYRenderer::finishFrame(
    score::gfx::RenderList& renderer, QRhiCommandBuffer& cb,
    QRhiResourceUpdateBatch*& res)
{
  cb.beginPass(m_renderTarget.renderTarget, Qt::black, {1.0f, 0}, res);
  res = nullptr;
  {
    const auto sz = renderer.state.renderSize;
    cb.setGraphicsPipeline(m_p.pipeline);
    cb.setShaderResources(m_p.srb);
    cb.setViewport(QRhiViewport(0, 0, sz.width(), sz.height()));

    const auto& mesh = renderer.defaultTriangle();
    mesh.draw(this->m_mesh, cb);
  }

  auto next = renderer.state.rhi->nextResourceUpdateBatch();

  QRhiReadbackDescription rb(m_renderTarget.texture);
  next->readBackTexture(rb, m_readback);
  cb.endPass(next);

  next = nullptr;
}

}
