#pragma once

#include <QPainter>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

namespace Gfx
{
class FlatHeaderButton : public QToolButton
{
  W_OBJECT(FlatHeaderButton)

public:
  using QToolButton::QToolButton;

protected:
  void paintEvent(QPaintEvent*) override
  {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const auto& pal = palette();

    if(underMouse())
      p.fillRect(rect(), pal.color(QPalette::Midlight));
    else if(hasFocus())
      p.fillRect(rect(), pal.color(QPalette::Midlight).darker(110));

    if(hasFocus())
    {
      p.setPen(pal.color(QPalette::Highlight));
      p.drawRect(rect().adjusted(0, 0, -1, -1));
    }

    const int arrowSize = 6;
    const int arrowX = 8;
    const int arrowY = (height() - arrowSize) / 2;

    p.setPen(Qt::NoPen);
    p.setBrush(pal.color(QPalette::WindowText));

    QPolygonF arrow;
    if(isChecked())
    {
      arrow << QPointF(arrowX, arrowY) << QPointF(arrowX + arrowSize, arrowY)
            << QPointF(arrowX + arrowSize / 2.0, arrowY + arrowSize);
    }
    else
    {
      arrow << QPointF(arrowX, arrowY)
            << QPointF(arrowX + arrowSize, arrowY + arrowSize / 2.0)
            << QPointF(arrowX, arrowY + arrowSize);
    }
    p.drawPolygon(arrow);

    p.setPen(pal.color(QPalette::WindowText));
    const int textLeft = arrowX + arrowSize + 8;
    QRect textRect(textLeft, 0, width() - textLeft, height());
    p.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, text());
  }
};

class CollapsibleSection : public QWidget
{
  W_OBJECT(CollapsibleSection)

public:
  explicit CollapsibleSection(const QString& title, QWidget* parent = nullptr)
      : QWidget(parent)
  {
    m_header = new FlatHeaderButton(this);
    m_header->setText(title);
    m_header->setCheckable(true);
    m_header->setChecked(true);
    m_header->setFocusPolicy(Qt::TabFocus);
    m_header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_header->setFixedHeight(24);

    m_contentWidget = new QWidget(this);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(m_header);
    mainLayout->addWidget(m_contentWidget);

    connect(m_header, &QToolButton::toggled, this, [this](bool checked) {
      m_contentWidget->setVisible(checked);
      m_header->update();
    });
  }

  void setContentLayout(QLayout* layout)
  {
    if(m_contentWidget->layout())
    {
      qWarning(
          "CollapsibleSection::setContentLayout: content layout already set, ignoring.");
      return;
    }
    layout->setContentsMargins(10, 8, 10, 8);
    m_contentWidget->setLayout(layout);
  }

  void setExpanded(bool expanded) { m_header->setChecked(expanded); }
  bool isExpanded() const { return m_header->isChecked(); }

private:
  FlatHeaderButton* m_header = nullptr;
  QWidget* m_contentWidget = nullptr;
};
}
