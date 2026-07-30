#pragma once

#include <QWidget>
#include <memory>

namespace bili {
class IConfig;
class IUserProfileFetcher;
} // namespace bili

namespace bili::gui {

class ImageLoader;

// 个人主页：展示头像、昵称、UID、注册时间、会员等级等。
// 依赖注入：IUserProfileFetcher* 与 ImageLoader* 均由外部装配并共享。
class ProfilePage : public QWidget {
    Q_OBJECT
public:
    explicit ProfilePage(bili::IConfig* config,
                         bili::IUserProfileFetcher* fetcher,
                         ImageLoader* imageLoader,
                         QWidget* parent = nullptr);
    ~ProfilePage() override;

    void refresh();

private slots:
    void onRefreshClicked();

private:
    class Impl;
    std::unique_ptr<Impl> d;
};

} // namespace bili::gui
