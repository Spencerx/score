#include <Vst3/ApplicationPlugin.hpp>
#include <Vst3/DataStream.hpp>
#include <Vst3/EditHandler.hpp>
#include <Vst3/Plugin.hpp>
#include <Vst3/UI/Linux/PlugFrame.hpp>
#include <Vst3/UI/PlugFrame.hpp>
#include <Vst3/UI/Window.hpp>

#include <ossia/detail/algorithms.hpp>

#include <QTimer>
#include <QWindow>

namespace vst3
{
using namespace Steinberg;

static Steinberg::Vst::IComponent*
createComponent(VST3::Hosting::Module& mdl, const std::string& name)
{
  const auto& factory = mdl.getFactory();
  for(auto& class_info : factory.classInfos())
    if(class_info.category() == kVstAudioEffectClass)
    {
      if(name.empty() || name == class_info.name())
      {
        Steinberg::Vst::IComponent* obj{};
        factory.get()->createInstance(
            class_info.ID().data(), Steinberg::Vst::IComponent::iid,
            reinterpret_cast<void**>(&obj));
        return obj;
      }
    }

  throw std::runtime_error(
      fmt::format("Couldn't create VST3 component ({})", mdl.getPath()));
}

static Steinberg::Vst::IComponent*
createComponent(VST3::Hosting::Module& mdl, const VST3::UID& cls)
{
  const auto& factory = mdl.getFactory();
  Steinberg::Vst::IComponent* obj{};
  factory.get()->createInstance(
      cls.data(), Steinberg::Vst::IComponent::iid, reinterpret_cast<void**>(&obj));
  return obj;

  throw std::runtime_error(
      fmt::format("Couldn't create VST3 component ({})", mdl.getPath()));
}

void Plugin::loadAudioProcessor(ApplicationPlugin& ctx)
{
  Steinberg::Vst::IAudioProcessor* processor_ptr = nullptr;
  auto audio_iface_res = component->queryInterface(
      Steinberg::Vst::IAudioProcessor::iid, (void**)&processor_ptr);
  if(audio_iface_res != Steinberg::kResultOk || !processor_ptr)
    throw std::runtime_error(
        fmt::format("Couldn't get VST3 AudioProcessor interface ({})", path));

  processor = processor_ptr;
}

void Plugin::loadBuses()
{
  audio_ins = component->getBusCount(Steinberg::Vst::kAudio, Steinberg::Vst::kInput);
  event_ins = component->getBusCount(Steinberg::Vst::kEvent, Steinberg::Vst::kInput);
  audio_outs = component->getBusCount(Steinberg::Vst::kAudio, Steinberg::Vst::kOutput);
  event_outs = component->getBusCount(Steinberg::Vst::kEvent, Steinberg::Vst::kOutput);
}

void Plugin::loadPresets()
{
  Steinberg::Vst::IUnitInfo* unit_ptr = nullptr;
  Steinberg::Vst::IProgramListData* pl_ptr = nullptr;
  Steinberg::Vst::IUnitData* udata_ptr = nullptr;
  {
    auto res
        = controller->queryInterface(Steinberg::Vst::IUnitInfo::iid, (void**)&unit_ptr);
    if(res != Steinberg::kResultOk || !unit_ptr)
      return;
  }

  {
    auto res = component->queryInterface(
        Steinberg::Vst::IProgramListData::iid, (void**)&pl_ptr);
    if(res != Steinberg::kResultOk || !pl_ptr)
      pl_ptr = nullptr;
  }

  {
    auto res
        = component->queryInterface(Steinberg::Vst::IUnitData::iid, (void**)&udata_ptr);
    if(res != Steinberg::kResultOk || !udata_ptr)
      udata_ptr = nullptr;
  }

  this->units = unit_ptr;

  int banks = this->units->getProgramListCount();
  if(banks > 0)
  {
    this->units->getProgramListInfo(0, this->programs);
  }
}

void Plugin::loadProcessorStateToController()
{
  // Copy the state from the processor component to the controller

  QByteArray arr;
  QDataStream write_stream{&arr, QIODevice::ReadWrite};
  Vst3DataStream stream{write_stream};
  // thanks steinberg doc not even up to date.... stream.setByteOrder (kLittleEndian);
  if(this->component->getState(&stream) == kResultTrue)
  {
    QDataStream read_stream{arr};
    Vst3DataStream stream{read_stream};
    controller->setComponentState(&stream);
  }
}

void Plugin::loadEditController(Model& model, ApplicationPlugin& ctx)
{
  Steinberg::Vst::IEditController* controller{};
  auto ctl_res = component->queryInterface(
      Steinberg::Vst::IEditController::iid, (void**)&controller);
  if(ctl_res != Steinberg::kResultOk || !controller)
  {
    Steinberg::TUID cid;
    if(component->getControllerClassId(cid) == Steinberg::kResultTrue)
    {
      FUID f{cid};
      mdl->getFactory().get()->createInstance(
          f, Steinberg::Vst::IEditController::iid, (void**)&controller);
    }
  }

  if(!controller)
  {
    qDebug() << "Couldn't get VST3 Controller interface : " << path.c_str();
    return;
  }

  controller->initialize(&ctx.m_host);
  this->controller = controller;

  // Connect the controller to the component... for... reasons
  {
    controller->setComponentHandler(new ComponentHandler{model});
    using namespace Steinberg;
    using namespace Steinberg::Vst;
    // TODO need disconnection

    IConnectionPoint* compICP{};
    IConnectionPoint* contrICP{};
    if(component->queryInterface(IConnectionPoint::iid, (void**)&compICP) != kResultOk)
      compICP = nullptr;

    if(controller->queryInterface(IConnectionPoint::iid, (void**)&contrICP) != kResultOk)
      contrICP = nullptr;

    if(compICP && contrICP)
    {
      compICP->connect(contrICP);
      contrICP->connect(compICP);
    }
  }
}

void Plugin::loadView(Model& model)
{
  if(model.fx.view)
    return;

  // Try to instantiate a view
  // r i d i c u l o u s

  this->ui_available = false;
  this->ui_owned = false;

  Steinberg::IPlugView* view{};
  if(!(view = controller->createView(Steinberg::Vst::ViewType::kEditor)))
  {
    if(!(view = controller->createView(nullptr)))
    {
      if((controller->queryInterface(IPlugView::iid, (void**)&view)
          == Steinberg::kResultOk)
         && view)
      {
        view->addRef();
        this->ui_owned = true;
      }
    }
  }

  // Create a widget to put the view inside
  if(view)
  {
    this->view = view;
    this->ui_available
        = view->isPlatformTypeSupported(currentPlatform()) == Steinberg::kResultTrue;
  }
}

void Plugin::load(
    Model& model, ApplicationPlugin& ctx, const std::string& path, const VST3::UID& uid,
    double sample_rate, int max_bs)
{
  this->path = path;
  mdl = ctx.getModule(path);
  if(!mdl)
    return;
  component = createComponent(*mdl, uid);
  if(!component)
    return;

  if(component->initialize(&ctx.m_host) != Steinberg::kResultOk)
    throw std::runtime_error(
        fmt::format("Couldn't initialize VST3 component ({})", path));

  loadAudioProcessor(ctx);

  loadEditController(model, ctx);

  loadView(model);

  loadBuses();

  loadPresets();

  start(sample_rate, max_bs);
}

void Plugin::start(double_t sample_rate, int max_bs)
{
  // Some level of introspection
  auto sampleSize = Steinberg::Vst::kSample32;
  if(processor->canProcessSampleSize(Steinberg::Vst::kSample64)
     == Steinberg::kResultTrue)
  {
    sampleSize = Steinberg::Vst::kSample64;
    supports_double = true;
  }

  Steinberg::Vst::ProcessSetup setup{
      Steinberg::Vst::kRealtime, sampleSize, max_bs, sample_rate};

  if(processor->setupProcessing(setup) != Steinberg::kResultOk)
    throw std::runtime_error(fmt::format("Couldn't setup VST3 processing ({})", path));

  if(component->setActive(true) != Steinberg::kResultOk)
    throw std::runtime_error(fmt::format("Couldn't set VST3 active ({})", path));
}

void Plugin::stop()
{
#if (!(defined(__APPLE__) || defined(_WIN32))) && __has_include(<xcb/xcb.h>)
  if(plugFrame)
  {
    plugFrame->cleanup();
  }
#endif
  if(!component)
    return;

  component->setActive(false);

  if(view)
  {
    view = nullptr;
  }

  if(controller)
  {
    controller->terminate();
    controller = nullptr;
  }

  if(processor)
  {
    processor = nullptr;
  }

  component->terminate();
  component = nullptr;

  if(plugFrame)
  {
    QTimer::singleShot(100, [ptr = plugFrame] { delete ptr; });
  }
}

Plugin::~Plugin() { }

}
