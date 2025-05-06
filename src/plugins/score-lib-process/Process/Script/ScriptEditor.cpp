#include "ScriptEditor.hpp"

#include "MultiScriptEditor.hpp"
#include "ScriptWidget.hpp"

#include <score/tools/FileWatch.hpp>
#include <score/widgets/SetIcons.hpp>

#include <QCodeEditor>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QTabWidget>
#include <QVBoxLayout>

namespace Process
{
ScriptDialog::ScriptDialog(
    const std::string_view language, const score::DocumentContext& ctx, QWidget* parent)
    : QDialog{parent}
    , m_context{ctx}
{
  this->setBaseSize(800, 300);
  this->setWindowFlag(Qt::WindowCloseButtonHint, false);
  auto lay = new QVBoxLayout{this};
  this->setLayout(lay);

  m_textedit = createScriptWidget(language);

  m_error = new QPlainTextEdit{this};
  m_error->setReadOnly(true);
  m_error->setMaximumHeight(120);
  m_error->setContextMenuPolicy(Qt::ContextMenuPolicy::CustomContextMenu);

  lay->addWidget(m_textedit);
  lay->addWidget(m_error);
  lay->setStretch(0, 3);
  lay->setStretch(1, 1);
  auto bbox = new QDialogButtonBox{
      QDialogButtonBox::Ok | QDialogButtonBox::Reset | QDialogButtonBox::Close, this};
  if(auto editorPath = QSettings{}.value("Skin/DefaultEditor").toString();
     !editorPath.isEmpty())
  {
    auto openExternalBtn = new QPushButton{tr("Edit in default editor"), this};
    connect(openExternalBtn, &QPushButton::clicked, this, [this, editorPath] {
      openInExternalEditor(editorPath);
    });
    bbox->addButton(openExternalBtn, QDialogButtonBox::HelpRole);
    openExternalBtn->setToolTip(
        tr("Edit in the default editor set in Score Settings > User Interface"));
    auto icon = makeIcons(
        QStringLiteral(":/icons/undock_on.png"),
        QStringLiteral(":/icons/undock_off.png"),
        QStringLiteral(":/icons/undock_off.png"));
    openExternalBtn->setIcon(icon);
  }
  bbox->button(QDialogButtonBox::Ok)->setText(tr("Compile"));
  bbox->button(QDialogButtonBox::Reset)->setText(tr("Clear log"));
  connect(bbox->button(QDialogButtonBox::Reset), &QPushButton::clicked, this, [this] {
    m_error->clear();
  });
  lay->addWidget(bbox);

  auto ce = qobject_cast<QCodeEditor*>(m_textedit);
  connect(ce, &QCodeEditor::livecodeTrigger, this, &ScriptDialog::on_accepted);
  connect(bbox, &QDialogButtonBox::accepted, this, &ScriptDialog::on_accepted);
  connect(bbox, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(
      bbox->button(QDialogButtonBox::Close), &QPushButton::clicked, this,
      &QDialog::close);
}

QString ScriptDialog::text() const noexcept
{
  return m_textedit->document()->toPlainText();
}

void ScriptDialog::setText(const QString& str)
{
  if(str != text())
  {
    m_textedit->setPlainText(str);
  }
}

void ScriptDialog::setError(int line, const QString& str)
{
  m_error->setPlainText(str);
}

MultiScriptDialog::MultiScriptDialog(const score::DocumentContext& ctx, QWidget* parent)
    : QDialog{parent}
    , m_context{ctx}
{
  this->setBaseSize(800, 300);
  this->setWindowFlag(Qt::WindowCloseButtonHint, false);
  auto lay = new QVBoxLayout{this};
  this->setLayout(lay);

  m_tabs = new QTabWidget;
  lay->addWidget(m_tabs);

  m_error = new QPlainTextEdit;
  m_error->setReadOnly(true);
  m_error->setMaximumHeight(120);
  m_error->setContextMenuPolicy(Qt::ContextMenuPolicy::CustomContextMenu);

  lay->addWidget(m_error);

  auto bbox = new QDialogButtonBox{
      QDialogButtonBox::Ok | QDialogButtonBox::Reset | QDialogButtonBox::Close, this};

  lay->addWidget(bbox);

  bbox->button(QDialogButtonBox::Ok)->setText(tr("Compile"));
  bbox->button(QDialogButtonBox::Reset)->setText(tr("Clear log"));
  connect(bbox->button(QDialogButtonBox::Reset), &QPushButton::clicked, this, [this] {
    m_error->clear();
  });

  connect(bbox, &QDialogButtonBox::accepted, this, &MultiScriptDialog::on_accepted);
  connect(bbox, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(
      bbox->button(QDialogButtonBox::Close), &QPushButton::clicked, this,
      &QDialog::close);

  if(auto editorPath = QSettings{}.value("Skin/DefaultEditor").toString();
     !editorPath.isEmpty())
  {
    auto openExternalBtn = new QPushButton{tr("Edit in default editor"), this};
    connect(openExternalBtn, &QPushButton::clicked, this, [this, editorPath] {
      openInExternalEditor(editorPath);
    });
    bbox->addButton(openExternalBtn, QDialogButtonBox::HelpRole);
    openExternalBtn->setToolTip(
        tr("Edit in the default editor set in Score Settings > User Interface"));
    auto icon = makeIcons(
        QStringLiteral(":/icons/undock_on.png"),
        QStringLiteral(":/icons/undock_off.png"),
        QStringLiteral(":/icons/undock_off.png"));
    openExternalBtn->setIcon(icon);
  }
}

void MultiScriptDialog::addTab(
    const QString& name, const QString& text, const std::string_view language)
{
  auto textedit = createScriptWidget(language);
  textedit->setText(text);
  auto ce = qobject_cast<QCodeEditor*>(textedit);
  connect(ce, &QCodeEditor::livecodeTrigger, this, &MultiScriptDialog::on_accepted);

  m_tabs->addTab(textedit, name);
  m_editors.push_back({textedit});
}

std::vector<QString> MultiScriptDialog::text() const noexcept
{
  std::vector<QString> vec;
  vec.reserve(m_editors.size());
  for(const auto& tab : m_editors)
    vec.push_back(tab.textedit->document()->toPlainText());
  return vec;
}

void MultiScriptDialog::setText(int idx, const QString& str)
{
  SCORE_ASSERT(idx >= 0);
  SCORE_ASSERT(std::size_t(idx) < m_editors.size());

  auto textEdit = m_editors[idx].textedit;
  if(str != textEdit->document()->toPlainText())
  {
    textEdit->setPlainText(str);
  }
}

void MultiScriptDialog::setError(const QString& str)
{
  m_error->setPlainText(str);
}

void MultiScriptDialog::clearError()
{
  m_error->clear();
}

void ScriptDialog::openInExternalEditor(const QString& editorPath)
{
#if QT_CONFIG(process)
  if(editorPath.isEmpty())
  {
    QMessageBox::warning(
        this, tr("Error"), tr("no 'Default editor' configured in score settings"));
    return;
  }

  const QString tempFile = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                           + "/ossia_script_temp.js";
  QFile file(tempFile);
  if(!file.open(QIODevice::WriteOnly))
  {
    QMessageBox::warning(this, tr("Error"), tr("failed to create temporary file."));
    return;
  }

  file.write(this->text().toUtf8());

  auto& w = score::FileWatch::instance();
  m_fileHandle = std::make_shared<std::function<void()>>([this, tempFile]() {
    QFile file(tempFile);
    if(file.open(QIODevice::ReadOnly))
    {
      QString updatedContent = QString::fromUtf8(file.readAll());

      QMetaObject::invokeMethod(m_textedit, [this, updatedContent]() {
        if(m_textedit)
        {
          m_textedit->setPlainText(updatedContent);
        }
      });
    }
  });
  w.add(tempFile, m_fileHandle);

  if(!QProcess::startDetached(editorPath, QStringList{tempFile}))
  {
    QMessageBox::warning(this, tr("Error"), tr("failed to launch external editor"));
  }
#endif
}

void ScriptDialog::stopWatchingFile(const QString& tempFile)
{
  if(tempFile.isEmpty())
  {
    return;
  }

  auto& w = score::FileWatch::instance();
  w.remove(tempFile, m_fileHandle);
  m_fileHandle.reset();
}

void MultiScriptDialog::openInExternalEditor(const QString& editorPath)
{
#if QT_CONFIG(process)
  if(editorPath.isEmpty())
  {
    QMessageBox::warning(
        this, tr("Error"), tr("no 'Default editor' configured in score settings"));
    return;
  }

  if(!QFile::exists(editorPath))
  {
    QMessageBox::warning(
        this, tr("Error"), tr("the configured external editor does not exist."));
    return;
  }

  QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
  QDir dir(tempDir);
  QStringList openedFiles;

  for(int i = 0; i < m_tabs->count(); ++i)
  {
    QString tabName = m_tabs->tabText(i);
    QString tempFile = tempDir + "/" + tabName + ".js";

    QFile file(tempFile);
    if(!file.open(QIODevice::WriteOnly))
    {
      QMessageBox::warning(this, tr("Error"), tr("failed to create temporary files."));
      return;
    }

    QWidget* widget = m_tabs->widget(i);
    QTextEdit* textedit = qobject_cast<QTextEdit*>(widget);

    if(textedit)
    {
      QString content = textedit->document()->toPlainText();
      file.write(content.toUtf8());
    }

    openedFiles.append(tempFile);
  }

  if(!openedFiles.isEmpty() && !QProcess::startDetached(editorPath, openedFiles))
  {
    QMessageBox::warning(this, tr("Error"), tr("Failed to launch external editor"));
  }
#endif
}

}
