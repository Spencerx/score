#pragma once
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>

#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/nodes/faust/faust_utils.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/network/domain/domain.hpp>

#include <QDebug>

#include <faust/gui/MetaDataUI.h>

#include <string_view>

namespace Faust
{

template <typename Proc, bool Synth>
struct UI
    : ::UI
    , MetaDataUI
{
  Proc& fx;

  UI(Proc& sfx)
      : fx{sfx}
  {
  }

  void openTabBox(const char* label) override { }
  void openHorizontalBox(const char* label) override { }
  void openVerticalBox(const char* label) override { }
  void closeBox() override { }
  void declare(FAUSTFLOAT* zone, const char* key, const char* val) override
  {
    MetaDataUI::declare(zone, key, val);
  }
  void
  addSoundfile(const char* label, const char* filename, Soundfile** sf_zone) override
  {
  }

  void addButton(const char* label, FAUSTFLOAT* zone) override
  {
    if constexpr(Synth)
    {
      using namespace std::literals;
      if(label == "Panic"sv || label == "gate"sv)
        return;
    }

    auto inl = new Process::Button{label, getStrongId(fx.inlets()), &fx};
    fx.inlets().push_back(inl);
  }

  void addCheckButton(const char* label, FAUSTFLOAT* zone) override
  {
    auto inl = new Process::Toggle{bool(*zone), label, getStrongId(fx.inlets()), &fx};
    fx.inlets().push_back(inl);
  }

  void addVerticalSlider(
      const char* label, FAUSTFLOAT* zone, FAUSTFLOAT init, FAUSTFLOAT min,
      FAUSTFLOAT max, FAUSTFLOAT step) override
  {
    using namespace std::literals;
    if constexpr(Synth)
    {
      if(label == "gain"sv || label == "freq"sv || label == "sustain"sv)
        return;
    }

    if(label == "0x00"sv)
      label = "Control";

    if(isKnob(zone))
    {
      auto inl = new Process::FloatKnob{
          (float)min, (float)max, (float)init, label, getStrongId(fx.inlets()), &fx};
      fx.inlets().push_back(inl);
    }
    else
    {
      auto inl = new Process::FloatSlider{
          (float)min, (float)max, (float)init, label, getStrongId(fx.inlets()), &fx};
      fx.inlets().push_back(inl);
    }
  }

  void addHorizontalSlider(
      const char* label, FAUSTFLOAT* zone, FAUSTFLOAT init, FAUSTFLOAT min,
      FAUSTFLOAT max, FAUSTFLOAT step) override
  {
    addVerticalSlider(label, zone, init, min, max, step);
  }

  void addNumEntry(
      const char* label, FAUSTFLOAT* zone, FAUSTFLOAT init, FAUSTFLOAT min,
      FAUSTFLOAT max, FAUSTFLOAT step) override
  {
    // TODO spinbox ?
    addVerticalSlider(label, zone, init, min, max, step);
  }

  void addHorizontalBargraph(
      const char* label, FAUSTFLOAT* zone, FAUSTFLOAT min, FAUSTFLOAT max) override
  {
    auto inl = new Process::Bargraph{
        (float)min, (float)max, (float)min, label, getStrongId(fx.outlets()), &fx};
    fx.outlets().push_back(inl);
  }

  void addVerticalBargraph(
      const char* label, FAUSTFLOAT* zone, FAUSTFLOAT min, FAUSTFLOAT max) override
  {
    addHorizontalBargraph(label, zone, min, max);
  }
};

template <typename Proc, bool SetInit, bool Synth>
struct UpdateUI
    : ::UI
    , MetaDataUI
{
  Proc& fx;
  Process::Inlets& toRemove;
  Process::Outlets& toRemoveO;
  std::size_t i = 1;
  std::size_t o = 1;

  UpdateUI(Proc& sfx, Process::Inlets& toRemove, Process::Outlets& toRemoveO)
      : fx{sfx}
      , toRemove{toRemove}
      , toRemoveO{toRemoveO}
  {
  }

  void openTabBox(const char* label) override { }
  void openHorizontalBox(const char* label) override { }
  void openVerticalBox(const char* label) override { }
  void closeBox() override { }
  void declare(FAUSTFLOAT* zone, const char* key, const char* val) override
  {
    MetaDataUI::declare(zone, key, val);
  }
  void
  addSoundfile(const char* label, const char* filename, Soundfile** sf_zone) override
  {
  }

  void replace(Process::Inlet*& oldinl, Process::Inlet* newinl)
  {
    toRemove.push_back(oldinl);
    oldinl = newinl;
  }

  void replace(Process::Outlet*& oldoutl, Process::Outlet* newoutl)
  {
    toRemoveO.push_back(oldoutl);
    oldoutl = newoutl;
  }

  void addButton(const char* label, FAUSTFLOAT* zone) override
  {
    if constexpr(Synth)
    {
      using namespace std::literals;
      if(label == "Panic"sv || label == "gate"sv)
        return;
    }

    if(i < fx.inlets().size())
    {
      if(auto inlet = dynamic_cast<Process::Button*>(fx.inlets()[i]))
      {
        inlet->setName(label);
      }
      else
      {
        auto id = fx.inlets()[i]->id();
        replace(fx.inlets()[i], new Process::Button{label, id, &fx});
      }
    }
    else
    {
      auto inl = new Process::Button{label, getStrongId(fx.inlets()), &fx};
      fx.inlets().push_back(inl);
      fx.controlAdded(inl->id());
    }
    i++;
  }

  void addCheckButton(const char* label, FAUSTFLOAT* zone) override
  {
    if(i < fx.inlets().size())
    {
      if(auto inlet = dynamic_cast<Process::Toggle*>(fx.inlets()[i]))
      {
        inlet->setName(label);
        if constexpr(SetInit)
          inlet->setValue(bool(*zone));
      }
      else
      {
        auto id = fx.inlets()[i]->id();
        replace(fx.inlets()[i], new Process::Toggle{bool(*zone), label, id, &fx});
      }
    }
    else
    {
      auto inl = new Process::Toggle{bool(*zone), label, getStrongId(fx.inlets()), &fx};
      fx.inlets().push_back(inl);
      fx.controlAdded(inl->id());
    }
    i++;
  }

  void addVerticalSlider(
      const char* label, FAUSTFLOAT* zone, FAUSTFLOAT init, FAUSTFLOAT min,
      FAUSTFLOAT max, FAUSTFLOAT step) override
  {
    using namespace std::literals;
    if constexpr(Synth)
    {
      if(label == "gain"sv || label == "freq"sv || label == "sustain"sv)
        return;
    }

    if(label == "0x00"sv)
      label = "Control";

    if(i < fx.inlets().size())
    {
      if(Process::FloatSlider
         * slider{dynamic_cast<Process::FloatSlider*>(fx.inlets()[i])})
      {
        if(isKnob(zone))
        {
          auto id = fx.inlets()[i]->id();
          auto inl = new Process::FloatKnob{
              (float)min, (float)max, (float)init, label, getStrongId(fx.inlets()), &fx};
          replace(fx.inlets()[i], inl);
        }
        else
        {
          slider->setName(label);
          slider->setDomain(ossia::make_domain(min, max));
          if constexpr(SetInit)
            slider->setValue(init);
        }
      }
      else if(
          Process::FloatKnob * knob{dynamic_cast<Process::FloatKnob*>(fx.inlets()[i])})
      {
        if(isKnob(zone))
        {
          knob->setName(label);
          knob->setDomain(ossia::make_domain(min, max));
          if constexpr(SetInit)
            knob->setValue(init);
        }
        else
        {
          auto id = fx.inlets()[i]->id();
          auto inl = new Process::FloatSlider{
              (float)min, (float)max, (float)init, label, getStrongId(fx.inlets()), &fx};
          replace(fx.inlets()[i], inl);
        }
      }
      else
      {
        auto id = fx.inlets()[i]->id();
        if(isKnob(zone))
        {
          auto inl = new Process::FloatKnob{
              (float)min, (float)max, (float)init, label, getStrongId(fx.inlets()), &fx};
          replace(fx.inlets()[i], inl);
        }
        else
        {
          auto inl = new Process::FloatSlider{
              (float)min, (float)max, (float)init, label, getStrongId(fx.inlets()), &fx};
          replace(fx.inlets()[i], inl);
        }
      }
    }
    else
    {
      if(isKnob(zone))
      {
        auto inl = new Process::FloatKnob{
            (float)min, (float)max, (float)init, label, getStrongId(fx.inlets()), &fx};
        fx.inlets().push_back(inl);
        fx.controlAdded(inl->id());
      }
      else
      {
        auto inl = new Process::FloatSlider{
            (float)min, (float)max, (float)init, label, getStrongId(fx.inlets()), &fx};
        fx.inlets().push_back(inl);
        fx.controlAdded(inl->id());
      }
    }

    i++;
  }

  void addHorizontalSlider(
      const char* label, FAUSTFLOAT* zone, FAUSTFLOAT init, FAUSTFLOAT min,
      FAUSTFLOAT max, FAUSTFLOAT step) override
  {
    addVerticalSlider(label, zone, init, min, max, step);
  }

  void addNumEntry(
      const char* label, FAUSTFLOAT* zone, FAUSTFLOAT init, FAUSTFLOAT min,
      FAUSTFLOAT max, FAUSTFLOAT step) override
  {
    addVerticalSlider(label, zone, init, min, max, step);
  }

  void addHorizontalBargraph(
      const char* label, FAUSTFLOAT* zone, FAUSTFLOAT min, FAUSTFLOAT max) override
  {
    if(o < fx.outlets().size())
    {
      if(auto outlet = dynamic_cast<Process::Bargraph*>(fx.outlets()[o]))
      {
        outlet->setName(label);
        outlet->setDomain(ossia::make_domain(min, max));
      }
      else
      {
        auto id = fx.outlets()[o]->id();
        auto inl
            = new Process::Bargraph{(float)min, (float)max, (float)min, label, id, &fx};
        replace(fx.outlets()[o], inl);
      }
    }
    else
    {
      auto inl = new Process::Bargraph{
          (float)min, (float)max, (float)min, label, getStrongId(fx.outlets()), &fx};
      fx.outlets().push_back(inl);
      fx.controlAdded(inl->id());
    }
    o++;
  }

  void addVerticalBargraph(
      const char* label, FAUSTFLOAT* zone, FAUSTFLOAT min, FAUSTFLOAT max) override
  {
    addHorizontalBargraph(label, zone, min, max);
  }
};
}
