#include "PresetItemModel.hpp"

#include <Process/ApplicationPlugin.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>
#include <Process/ProcessMimeSerialization.hpp>

#include <Library/LibraryInterface.hpp>
#include <Library/LibrarySettings.hpp>
#include <Library/ProcessWidget.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/tools/File.hpp>

#include <core/document/Document.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <ossia/detail/ssize.hpp>

#include <QFile>
#include <QPainter>

namespace Library
{

PresetItemModel::PresetItemModel(
    const score::GUIApplicationContext& ctx, QObject* parent)
    : QAbstractItemModel{parent}
    , presets{ctx.applicationPlugin<Process::ApplicationPlugin>().presets}
{
}

QModelIndex PresetItemModel::index(int row, int column, const QModelIndex& parent) const
{
  return createIndex(row, column, nullptr);
}

QModelIndex PresetItemModel::parent(const QModelIndex& child) const
{
  return {};
}

int PresetItemModel::rowCount(const QModelIndex& parent) const
{
  return presets.size();
}

int PresetItemModel::columnCount(const QModelIndex& parent) const
{
  return 1;
}

QVariant PresetItemModel::data(const QModelIndex& index, int role) const
{
  if(index.row() < std::ssize(presets))
  {
    switch(role)
    {
      case Qt::DisplayRole:
      case Qt::EditRole:
        return presets[index.row()].name;
    }
  }

  return {};
}

PresetListView::~PresetListView() = default;
auto preset_list_view_symbol_for_shlib_bug = &PresetListView::doubleClicked;
void PresetListView::mouseDoubleClickEvent(QMouseEvent* event)
{
  auto proxy_index = indexAt(event->pos());
  if(!proxy_index.isValid())
    return;

  auto proxy = static_cast<QSortFilterProxyModel*>(this->model());
  auto model_index = proxy->mapToSource(proxy_index);

  auto& self = *safe_cast<PresetItemModel*>(proxy->sourceModel());
  if(model_index.row() < 0 || model_index.row() >= int(self.presets.size()))
    return;

  auto& preset = self.presets[model_index.row()];
  doubleClicked(preset);

  event->accept();
}

void PresetListView::paintEvent(QPaintEvent* e)
{
  QListView::paintEvent(e);

  if(model() && model()->rowCount(rootIndex()) > 0)
    return;

  QPainter p{this->viewport()};
  p.drawText(rect(), Qt::AlignLeft, "<No presets>");
}

static bool isValidForFilename(const QString& name)
{
  if(name.isEmpty())
    return false;

  for(QChar c : QStringLiteral(",^@=+{}[]~!?:&*\"|#%<>$\"';`'"))
    if(name.contains(c))
      return false;

  return true;
}

static bool updatePresetFilename(Process::Preset& preset, QString old = {})
{
  const auto& ctx = score::GUIAppContext();

  const auto& procs = ctx.interfaces<Process::ProcessFactoryList>();
  auto& factory = *procs.get(preset.key.key);
  const auto& desc = factory.descriptor(preset.key.effect); // TODO
  QString presetFolder = ctx.settings<Library::Settings::Model>().getUserPresetsPath();

  presetFolder += "/" + factory.prettyName();

  // Put VST, etc. in subfolders
  if(desc.prettyName != factory.prettyName())
    presetFolder += "/" + desc.prettyName;

  QDir{}.mkpath(presetFolder);
  QString presetPath = presetFolder + "/" + preset.name + ".scp";

  // FIXME refactor with score::addUniqueSuffix
  if(QFile::exists(presetPath))
  {
    presetPath = presetFolder + "/" + preset.name + " (%1).scp";

    int idx = 1;
    while(QFile::exists(presetPath.arg(idx)))
      idx++;
    presetPath = presetPath.arg(idx);
    preset.name = preset.name + QString("(%1)").arg(idx);
  }

  QFile f{presetPath};
  if(!f.open(QIODevice::WriteOnly))
    return false;

  f.write(preset.toJson());

  if(!old.isEmpty())
  {
    // Remove the old file
    const QString oldPath = presetFolder + "/" + old + ".scp";
    QFile::remove(oldPath);
  }

  return true;
}

bool PresetItemModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
  if(!index.isValid())
    return false;
  if(index.row() < 0 || index.row() >= int(presets.size()))
    return false;
  if(role != Qt::EditRole)
    return false;
  auto str = value.toString();
  if(!isValidForFilename(str))
    return false;

  auto& preset = presets[index.row()];

  auto old = preset.name;
  preset.name = str;
  if(!updatePresetFilename(preset, old))
  {
    preset.name = old;
    return false;
  }

  return true;
}

bool PresetItemModel::dropMimeData(
    const QMimeData* data, Qt::DropAction act, int row, int col,
    const QModelIndex& parent)
{
  const auto& ctx = score::GUIAppContext();
  const rapidjson::Document jsondoc = readJson(data->data(score::mime::layerdata()));
  if(!jsondoc.HasMember("Path"))
    return false;
  auto obj = JsonValue{jsondoc}["Path"].to<Path<Process::ProcessModel>>();
  auto doc = ctx.docManager.currentDocument();
  if(!doc)
    return false;
  const Process::ProcessModel* proc = obj.try_find(doc->context());
  if(!proc)
    return false;

  return savePreset(*proc);
}

bool PresetItemModel::savePreset(const Process::ProcessModel& proc)
{
  auto preset = proc.savePreset();
  if(!updatePresetFilename(preset))
    return false;

  beginResetModel();
  // beginInsertRows(QModelIndex(), presets.size(), presets.size());
  auto it = std::lower_bound(
      presets.begin(), presets.end(), preset,
      [](const Process::Preset& lhs, const Process::Preset& rhs) {
    return lhs.key < rhs.key;
      });

  presets.insert(it, std::move(preset));
  // endInsertRows();
  endResetModel();

  return true;
}

bool PresetItemModel::canDropMimeData(
    const QMimeData* data, Qt::DropAction act, int row, int col,
    const QModelIndex& parent) const
{
  return data->hasFormat(score::mime::layerdata());
}

QMimeData* PresetItemModel::mimeData(const QModelIndexList& indexes) const
{
  if(indexes.empty())
    return nullptr;

  int row = indexes.front().row();
  if(row >= int(presets.size()) || row < 0)
    return nullptr;

  auto mime = new QMimeData;
  const auto& preset = presets[row];

  mime->setData(score::mime::processpreset(), preset.toJson());

  return mime;
}

Qt::DropActions PresetItemModel::supportedDragActions() const
{
  return Qt::CopyAction;
}

Qt::DropActions PresetItemModel::supportedDropActions() const
{
  return Qt::MoveAction | Qt::CopyAction | Qt::LinkAction | Qt::IgnoreAction;
}

Qt::ItemFlags PresetItemModel::flags(const QModelIndex& index) const
{
  Qt::ItemFlags f = QAbstractItemModel::flags(index);
  f |= Qt::ItemIsDragEnabled;
  f |= Qt::ItemIsDropEnabled;
  f |= Qt::ItemIsEditable;
  return f;
}

bool PresetFilterProxy::filterAcceptsRow(int srcRow, const QModelIndex& srcParent) const
{
  PresetItemModel* model = safe_cast<PresetItemModel*>(sourceModel());
  if(model->presets[srcRow].key.key != currentFilter.key)
    return false;
  if(currentFilter.effect.isEmpty())
    return true;
  return model->presets[srcRow].key.effect == currentFilter.effect;
}

}
