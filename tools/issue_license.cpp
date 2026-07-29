// 开发者工具：签发离线激活码（收款后运行，生成激活码发给用户）
//
// 用法示例：
//   # 月付会员（30 天）
//   build/bin/BiliHistoryIssueLicense --type month
//   # 月付，自定义天数
//   build/bin/BiliHistoryIssueLicense --type month --days 31
//   # 买断（永久）
//   build/bin/BiliHistoryIssueLicense --type buyout
//   # 绑定用户机器码
//   build/bin/BiliHistoryIssueLicense --type buyout --machine 1a2b3c4d5e6f7890
//
// 输出：一行激活码字符串，直接发给用户。

#include <iostream>
#include <string>

#include <openssl/evp.h>
#include <openssl/pem.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QIODevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimeZone>
#include <QUuid>

namespace {

const char* PrivateKeyPath = "tools/private_key.pem";

QString base64UrlEncode(const QByteArray& data)
{
    return QString::fromUtf8(data.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

struct Args {
    QString type;
    int days = 30;
    QString machineId;
    QString lid;
};

Args parseArgs(int argc, char* argv[])
{
    Args args;
    for (int i = 1; i < argc; ++i) {
        const std::string arg(argv[i]);
        if (arg == "--type" && i + 1 < argc) {
            args.type = QString::fromUtf8(argv[++i]);
        } else if (arg == "--days" && i + 1 < argc) {
            args.days = std::stoi(argv[++i]);
        } else if (arg == "--machine" && i + 1 < argc) {
            args.machineId = QString::fromUtf8(argv[++i]);
        } else if (arg == "--id" && i + 1 < argc) {
            args.lid = QString::fromUtf8(argv[++i]);
        }
    }
    return args;
}

QByteArray signPayload(const QByteArray& payload, EVP_PKEY* pkey)
{
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        throw std::runtime_error("创建签名上下文失败");
    }

    if (EVP_DigestSignInit(ctx, nullptr, EVP_sha256(), nullptr, pkey) != 1) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("初始化签名失败");
    }

    if (EVP_DigestSignUpdate(ctx, payload.constData(), static_cast<size_t>(payload.size())) != 1) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("签名更新失败");
    }

    size_t sigLen = 0;
    if (EVP_DigestSignFinal(ctx, nullptr, &sigLen) != 1) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("获取签名长度失败");
    }

    QByteArray signature(static_cast<int>(sigLen), 0);
    if (EVP_DigestSignFinal(ctx,
                            reinterpret_cast<unsigned char*>(signature.data()),
                            &sigLen) != 1) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("签名失败");
    }
    signature.resize(static_cast<int>(sigLen));

    EVP_MD_CTX_free(ctx);
    return signature;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    const Args args = parseArgs(argc, argv);
    if (args.type != "month" && args.type != "buyout") {
        std::cerr << "用法: --type month|buyout [--days N] [--machine MID] [--id LID]" << std::endl;
        return 1;
    }

    QFile keyFile(PrivateKeyPath);
    if (!keyFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        std::cerr << "未找到私钥 " << PrivateKeyPath << "，请先运行 keygen" << std::endl;
        return 1;
    }
    const QByteArray keyPem = keyFile.readAll();
    keyFile.close();

    BIO* bio = BIO_new_mem_buf(keyPem.constData(), keyPem.size());
    if (!bio) {
        std::cerr << "创建 BIO 失败" << std::endl;
        return 1;
    }
    EVP_PKEY* pkey = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    if (!pkey) {
        std::cerr << "私钥格式无效" << std::endl;
        return 1;
    }

    const qint64 now = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();
    const qint64 exp = (args.type == "buyout") ? -1 : now + args.days * 86400;
    const QString lid = args.lid.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces).left(12) : args.lid;

    QJsonObject payloadObj;
    payloadObj[QStringLiteral("lid")] = lid;
    payloadObj[QStringLiteral("typ")] = args.type;
    payloadObj[QStringLiteral("iat")] = now;
    if (exp >= 0) {
        payloadObj[QStringLiteral("exp")] = exp;
    }
    if (!args.machineId.isEmpty()) {
        payloadObj[QStringLiteral("mid")] = args.machineId;
    }

    const QByteArray payload = QJsonDocument(payloadObj).toJson(QJsonDocument::Compact);

    QByteArray signature;
    try {
        signature = signPayload(payload, pkey);
    } catch (const std::exception& e) {
        EVP_PKEY_free(pkey);
        std::cerr << e.what() << std::endl;
        return 1;
    }
    EVP_PKEY_free(pkey);

    const QString code = base64UrlEncode(payload) + QStringLiteral(".") + base64UrlEncode(signature);

    const QString expiryText = (exp < 0)
                                   ? QStringLiteral("永久")
                                   : QDateTime::fromSecsSinceEpoch(exp, QTimeZone::UTC).toString(QStringLiteral("yyyy-MM-dd hh:mm"));

    std::cout << "=" << std::string(60, '=') << std::endl;
    std::cout << "类型: " << args.type.toStdString()
              << "   到期: " << expiryText.toStdString()
              << (args.machineId.isEmpty() ? "   未绑定机器码" : "   绑定机器码: " + args.machineId.toStdString())
              << std::endl;
    std::cout << "许可ID: " << lid.toStdString() << std::endl;
    std::cout << "激活码（发给用户）：" << std::endl;
    std::cout << code.toStdString() << std::endl;
    std::cout << "=" << std::string(60, '=') << std::endl;

    return 0;
}
