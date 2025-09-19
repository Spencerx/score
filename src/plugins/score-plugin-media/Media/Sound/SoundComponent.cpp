#include "SoundComponent.hpp"

#include <Process/ExecutionContext.hpp>
#include <Process/ExecutionSetup.hpp>
#include <Process/ExecutionTransaction.hpp>

#include <Scenario/Execution/score2OSSIA.hpp>

#include <score/tools/Bind.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/nodes/dummy.hpp>
#include <ossia/dataflow/nodes/sound.hpp>
#include <ossia/dataflow/nodes/sound_libav.hpp>
#include <ossia/dataflow/nodes/sound_mmap.hpp>
#include <ossia/dataflow/nodes/sound_ref.hpp>
#include <ossia/detail/pod_vector.hpp>

namespace
{
static std::unique_ptr<ossia::resampler>
make_resampler(ossia::audio_stretch_mode mode, const Media::AudioFile& f) noexcept
{
  auto res = std::make_unique<ossia::resampler>();
  const auto channels = f.channels();
  const auto sampleRate = f.sampleRate();
  res->reset(0, mode, channels, sampleRate);
  return res;
}

}
namespace Media
{

class SoundComponentSetup
{
public:
  static void construct(Execution::SoundComponent& component)
  {
    Sound::ProcessModel& element = component.process();
    auto handle = element.file();

    struct
    {
      Execution::SoundComponent& component;
      const AudioFile& file;
      void operator()(ossia::monostate) const noexcept
      {
        // Note : we need to construct a dummy node in case there is nothing,
        // because else the parent interval will destroy the component so
        // even if the sound is created afterwards there won't be any component
        // anymore

        auto node = std::make_shared<ossia::dummy_sound_node>();
        component.node = node;
        component.m_ossia_process = std::make_shared<ossia::sound_process>(node);
      }
      void
      operator()(const std::shared_ptr<Media::AudioFile::LibavReader>& r) const noexcept
      {
        Execution::Transaction commands{component.system()};

        auto node = ossia::make_node<ossia::nodes::sound_ref>(
            *component.system().execState.get());
        component.node = node;
        component.m_ossia_process = std::make_shared<ossia::sound_process>(node);
        update_ref(node, file, r, component, commands);

        commands.run_all();
      }
      void operator()(const Media::AudioFile::LibavStreamReader& r) const noexcept
      {
        Execution::Transaction commands{component.system()};

        auto node = ossia::make_node<ossia::nodes::sound_libav>(
            *component.system().execState.get());
        component.node = node;
        component.m_ossia_process = std::make_shared<ossia::sound_process>(node);
        update_libav(node, file, r, component, commands);

        commands.run_all();
      }
      void operator()(const Media::AudioFile::SndfileReader& r) const noexcept
      {
        Execution::Transaction commands{component.system()};

        auto node = ossia::make_node<ossia::nodes::sound_ref>(
            *component.system().execState.get());
        component.node = node;
        component.m_ossia_process = std::make_shared<ossia::sound_process>(node);
        update_sndfile(node, file, r, component, commands);

        commands.run_all();
      }
      void operator()(const Media::AudioFile::MmapReader& r) const noexcept
      {
        Execution::Transaction commands{component.system()};

        auto node = ossia::make_node<ossia::nodes::sound_mmap>(
            *component.system().execState.get());
        component.node = node;
        component.m_ossia_process = std::make_shared<ossia::sound_process>(node);
        update_mmap(node, file, r, component, commands);

        commands.run_all();
      }
    } _{component, *handle};
    ossia::apply(_, handle->m_impl);
  }

  static void update(Execution::SoundComponent& component)
  {
    Sound::ProcessModel& element = component.process();
    std::shared_ptr<AudioFile> handle = element.file();

    struct
    {
      Execution::SoundComponent& component;
      AudioFile& file;
      void operator()(ossia::monostate) const noexcept { return; }

      void replace_node(
          const std::shared_ptr<ossia::graph_node>& old_node,
          const std::shared_ptr<ossia::graph_node>& n,
          Execution::Transaction& commands) const noexcept
      {
        OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Ui);
        auto& setup = component.system().setup;
        auto& proc = component.process();
        for(auto& c : proc.outlet->cables())
        {
          setup.removeCable(c.find(component.system().context().doc), commands);
        }
        setup.unregister_node(component.process(), old_node, commands);
        setup.register_node(component.process(), n, commands);
        setup.replace_node(component.OSSIAProcessPtr(), n, commands);
        component.nodeChanged(old_node, n, &commands);
        component.node = n;
        for(auto& c : proc.outlet->cables())
        {
          setup.connectCable(c.find(component.system().context().doc), commands);
        }
      }

      void
      operator()(const std::shared_ptr<Media::AudioFile::LibavReader>& r) const noexcept
      {
        Execution::Transaction commands{component.system()};
        auto old_node = component.node;

        if(auto nr = std::dynamic_pointer_cast<ossia::nodes::sound_ref>(old_node))
        {
          update_ref(nr, file, r, component, commands);
        }
        else
        {
          auto n = ossia::make_node<ossia::nodes::sound_ref>(
              *component.system().execState.get());
          update_ref(n, file, r, component, commands);
          this->replace_node(old_node, n, commands);
        }

        commands.run_all();
      }
      void operator()(const Media::AudioFile::LibavStreamReader& r) const noexcept
      {
        Execution::Transaction commands{component.system()};
        auto old_node = component.node;

        if(auto nr = std::dynamic_pointer_cast<ossia::nodes::sound_libav>(old_node))
        {
          update_libav(nr, file, r, component, commands);
        }
        else
        {
          auto n = ossia::make_node<ossia::nodes::sound_libav>(
              *component.system().execState.get());
          update_libav(n, file, r, component, commands);
          this->replace_node(old_node, n, commands);
        }

        commands.run_all();
      }
      void operator()(const Media::AudioFile::SndfileReader& r) const noexcept
      {
        Execution::Transaction commands{component.system()};
        auto old_node = component.node;

        if(auto nr = std::dynamic_pointer_cast<ossia::nodes::sound_ref>(old_node))
        {
          update_sndfile(nr, file, r, component, commands);
        }
        else
        {
          auto n = ossia::make_node<ossia::nodes::sound_ref>(
              *component.system().execState.get());
          update_sndfile(n, file, r, component, commands);
          this->replace_node(old_node, n, commands);
        }

        commands.run_all();
      }
      void operator()(const Media::AudioFile::MmapReader& r) const noexcept
      {
        if(!r.wav)
          return;

        Execution::Transaction commands{component.system()};
        auto old_node = component.node;

        if(auto nm = std::dynamic_pointer_cast<ossia::nodes::sound_mmap>(old_node))
        {
          update_mmap(nm, file, r, component, commands);
        }
        else
        {
          auto n = ossia::make_node<ossia::nodes::sound_mmap>(
              *component.system().execState.get());
          update_mmap(n, file, r, component, commands);
          this->replace_node(old_node, n, commands);
        }

        commands.run_all();
      }
    } _{component, *handle};

    ossia::apply(_, handle->m_impl);
  }

  static void update_libav(
      const std::shared_ptr<ossia::nodes::sound_libav>& n, const AudioFile& handle,
      const Media::AudioFile::LibavStreamReader& r, Execution::SoundComponent& component,
      Execution::Transaction& commands)
  {
    auto& p = component.process();
    commands.push_back([n, r = r, samplerate = component.system().execState->sampleRate,
                        tempo = component.process().nativeTempo(),
                        res = make_resampler(component.process().stretchMode(), handle),
                        upmix = p.upmixChannels(), start = p.startChannel()]() mutable {
      ossia::libav_handle h;
      h.open(r.path, r.stream, samplerate);

      n->set_sound(std::move(h));
      n->set_start(start);
      n->set_upmix(upmix);
      n->set_resampler(std::move(*res));
      n->set_native_tempo(tempo);
    });
  }

  static void update_ref(
      const std::shared_ptr<ossia::nodes::sound_ref>& n, const AudioFile& handle,
      const std::shared_ptr<Media::AudioFile::LibavReader>& r,
      Execution::SoundComponent& component, Execution::Transaction& commands)
  {
    auto& p = component.process();
    commands.push_back([n, data = r->handle, channels = r->decoder.channels,
                        sampleRate = r->decoder.convertedSampleRate,
                        tempo = component.process().nativeTempo(),
                        res = make_resampler(component.process().stretchMode(), handle),
                        upmix = p.upmixChannels(), start = p.startChannel()]() mutable {
      n->set_sound(std::move(data), channels, sampleRate);
      n->set_start(start);
      n->set_upmix(upmix);
      n->set_resampler(std::move(*res));
      n->set_native_tempo(tempo);
    });
  }

  static void update_sndfile(
      const std::shared_ptr<ossia::nodes::sound_ref>& n, const AudioFile& handle,
      const Media::AudioFile::SndfileReader& r, Execution::SoundComponent& component,
      Execution::Transaction& commands)
  {
    auto& p = component.process();
    commands.push_back([n, data = r.handle, channels = r.decoder.channels,
                        sampleRate = r.decoder.convertedSampleRate,
                        tempo = component.process().nativeTempo(),
                        res = make_resampler(component.process().stretchMode(), handle),
                        upmix = p.upmixChannels(), start = p.startChannel()]() mutable {
      n->set_sound(std::move(data), channels, sampleRate);
      n->set_start(start);
      n->set_upmix(upmix);
      n->set_resampler(std::move(*res));
      n->set_native_tempo(tempo);
    });
  }

  static void update_mmap(
      const std::shared_ptr<ossia::nodes::sound_mmap>& n, const AudioFile& handle,
      const Media::AudioFile::MmapReader& r, Execution::SoundComponent& component,
      Execution::Transaction& commands)
  {
    auto& p = component.process();
    commands.push_back([n, data = r.wav, channels = r.wav.channels(),
                        tempo = component.process().nativeTempo(),
                        res = make_resampler(component.process().stretchMode(), handle),
                        upmix = p.upmixChannels(), start = p.startChannel()]() mutable {
      n->set_sound(std::move(data));
      n->set_start(start);
      n->set_upmix(upmix);
      n->set_resampler(std::move(*res));
      n->set_native_tempo(tempo);
    });
  }
};
}
namespace Execution
{

SoundComponent::SoundComponent(
    Media::Sound::ProcessModel& element, const Execution::Context& ctx, QObject* parent)
    : Execution::ProcessComponent_T<
        Media::Sound::ProcessModel,
        ossia::node_process>{element, ctx, "Executor::SoundComponent", parent}
    , m_recomputer{*this}
{
  Media::SoundComponentSetup{}.construct(*this);
  connect(
      &element, &Media::Sound::ProcessModel::fileChanged, this,
      &SoundComponent::on_fileChanged);

  auto node_action = [this](auto&& f) {
    if(auto n_ref = std::dynamic_pointer_cast<ossia::nodes::sound_ref>(this->node))
      in_exec([n_ref, ff = std::move(f)]() mutable { ff(*n_ref); });
    else if(
        auto n_mmap = std::dynamic_pointer_cast<ossia::nodes::sound_mmap>(this->node))
      in_exec([n_mmap, ff = std::move(f)]() mutable { ff(*n_mmap); });
    else if(
        auto n_lav = std::dynamic_pointer_cast<ossia::nodes::sound_libav>(this->node))
      in_exec([n_lav, ff = std::move(f)]() mutable { ff(*n_lav); });
  };

  con(element, &Media::Sound::ProcessModel::startChannelChanged, this, [=, &element] {
    node_action([start = element.startChannel()](auto& node) { node.set_start(start); });
  });
  con(element, &Media::Sound::ProcessModel::upmixChannelsChanged, this, [=, &element] {
    node_action(
        [start = element.upmixChannels()](auto& node) { node.set_upmix(start); });
  });
  con(element, &Media::Sound::ProcessModel::nativeTempoChanged, this, [=, &element] {
    node_action(
        [start = element.nativeTempo()](auto& node) { node.set_native_tempo(start); });
  });
  con(element, &Media::Sound::ProcessModel::stretchModeChanged, this, [=, &element] {
    node_action([r = make_resampler(element.stretchMode(), *element.file())](
                    auto& node) mutable { node.set_resampler(std::move(*r)); });
  });

  if(auto& file = element.file())
  {
    file->on_finishedDecoding.connect<&SoundComponent::Recomputer::recompute>(
        m_recomputer);
  }
}
void SoundComponent::on_fileChanged()
{
  m_recomputer.~Recomputer();
  new(&m_recomputer) Recomputer{*this};

  if(auto& file = process().file())
  {
    if(file->finishedDecoding())
    {
      recompute();
    }
    else
    {
      file->on_finishedDecoding.connect<&SoundComponent::Recomputer::recompute>(
          m_recomputer);
    }
  }
  else
  {
    recompute();
  }
}

void SoundComponent::recompute()
{
  Media::SoundComponentSetup{}.update(*this);
}

SoundComponent::~SoundComponent() { }
}
