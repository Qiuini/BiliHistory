#pragma once

#include <QFrame>
#include <QWidget>

#include "core/models.h"

#include <memory>

class QGridLayout;
class QScrollArea;

namespace bili::gui {

class ImageLoader;

class FollowingCard : public QFrame {
    Q_OBJECT
public:
    explicit FollowingCard(const FollowingUser& user, ImageLoader* loader, QWidget* parent = nullptr);
};

class FollowingPage : public QWidget {
    Q_OBJECT
public:
    // loader 由外部共享注入（典型来源：MainWindow::Impl 持有的单一 ImageLoader）。
    explicit FollowingPage(ImageLoader* loader, QWidget* parent = nullptr);
    ~FollowingPage() override;

    void loadData(const FollowingList& records);

private:
    class Impl;
    std::unique_ptr<Impl> d;
};

} // namespace bili::gui
