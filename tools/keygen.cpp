// 开发者工具：生成 RSA-2048 密钥对（仅需运行一次）
//
// 产物：
//   - tools/private_key.pem   私钥（务必保密、勿入库、勿打包进客户端）
//   - 终端打印公钥 PEM，将其粘贴到 src/licensing/keys.cpp 的 PublicKeyPem
//
// 若 tools/private_key.pem 已存在，默认不覆盖（除非加 --force）。

#include <iostream>
#include <string>

#include <openssl/evp.h>
#include <openssl/pem.h>

#include <QCoreApplication>
#include <QFile>

namespace {

const char* PrivateKeyPath = "tools/private_key.pem";

bool fileExists(const char* path)
{
    QFile file(path);
    return file.exists();
}

bool writePem(const char* path, const QByteArray& data)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        std::cerr << "无法写入文件: " << path << std::endl;
        return false;
    }
    file.write(data);
    file.close();
    return true;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    bool force = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--force") {
            force = true;
        }
    }

    if (fileExists(PrivateKeyPath) && !force) {
        std::cout << "[跳过] 私钥已存在: " << PrivateKeyPath << std::endl;
        std::cout << "       如需重新生成请加 --force（注意：旧激活码将全部失效）" << std::endl;
        return 0;
    }

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
    if (!ctx) {
        std::cerr << "创建 RSA 上下文失败" << std::endl;
        return 1;
    }

    if (EVP_PKEY_keygen_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        std::cerr << "初始化 RSA 密钥生成失败" << std::endl;
        return 1;
    }

    if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        std::cerr << "设置 RSA 密钥长度失败" << std::endl;
        return 1;
    }

    EVP_PKEY* pkey = nullptr;
    if (EVP_PKEY_keygen(ctx, &pkey) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        std::cerr << "生成 RSA 密钥对失败" << std::endl;
        return 1;
    }
    EVP_PKEY_CTX_free(ctx);

    BIO* privBio = BIO_new(BIO_s_mem());
    if (!privBio || PEM_write_bio_PrivateKey(privBio, pkey, nullptr, nullptr, 0, nullptr, nullptr) != 1) {
        EVP_PKEY_free(pkey);
        std::cerr << "导出私钥失败" << std::endl;
        return 1;
    }
    char* privData = nullptr;
    long privLen = BIO_get_mem_data(privBio, &privData);
    QByteArray privatePem(privData, static_cast<int>(privLen));
    BIO_free(privBio);

    BIO* pubBio = BIO_new(BIO_s_mem());
    if (!pubBio || PEM_write_bio_PUBKEY(pubBio, pkey) != 1) {
        EVP_PKEY_free(pkey);
        std::cerr << "导出公钥失败" << std::endl;
        return 1;
    }
    char* pubData = nullptr;
    long pubLen = BIO_get_mem_data(pubBio, &pubData);
    QByteArray publicPem(pubData, static_cast<int>(pubLen));
    BIO_free(pubBio);

    EVP_PKEY_free(pkey);

    if (!writePem(PrivateKeyPath, privatePem)) {
        return 1;
    }

    std::cout << "[完成] 私钥已写入: " << PrivateKeyPath << std::endl;
    std::cout << "=" << std::string(60, '=') << std::endl;
    std::cout << "请将下面公钥 PEM 粘贴到 src/licensing/keys.cpp 的 PublicKeyPem：" << std::endl;
    std::cout << publicPem.constData();
    std::cout << "=" << std::string(60, '=') << std::endl;

    return 0;
}
