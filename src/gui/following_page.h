#pragma once

#include <QFrame>
#include <QGridLayout>
#include <QScrollArea>
#include <QWidget>

#include "core/models.h"
#include "image_loader.h"

namespace bili::gui {

class FollowingCard : public QFrame {
    Q_OBJECT
public:
    explicit FollowingCard(const FollowingUser& user, ImageLoader* loader, QWidget* parent = nullptr);
};

class FollowingPage : public QWidget {
    Q_OBJECT
public:
    explicit FollowingPage(QWidget* parent = nullptr);
    void loadData(const FollowingList& records);

private:
    QScrollArea* m_scroll = nullptr;
    QWidget* m_container = nullptr;
    QGridLayout* m_grid = nullptr;
    ImageLoader* m_loader = nullptr;
};

} // namespace bili::gui
