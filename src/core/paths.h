#pragma once

#include <QDir>
#include <QString>

namespace bili {

class Paths {
public:
    static QString appName();
    static QString organization();

    // 用户数据根目录
    static QString appDataDir();

    // 配置目录
    static QString configDir();

    // CSV 主文件路径
    static QString historyCsvPath();

    // 快照备份目录
    static QString backupsDir();

    // 日志目录
    static QString logsDir();

    // 图片缓存目录
    static QString imageCacheDir();

    // 私密数据文件
    static QString secretsPath();

    // 授权文件
    static QString licensePath();

    // 临时目录
    static QString tempDir();

    // 生成带时间戳的备份文件名
    static QString backupFileName(const QString& baseName);
};

} // namespace bili
