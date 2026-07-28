"""
管理员工具：通过授权服务器签发新的激活码

用法示例：
    # 签发 1 个月付激活码（默认 30 天）
    python tools/issue_server_license.py --admin-key <key> --url http://127.0.0.1:8787

    # 签发 5 个月付激活码
    python tools/issue_server_license.py --admin-key <key> --count 5

    # 签发永久买断激活码
    python tools/issue_server_license.py --admin-key <key> --type buyout --count 3

环境变量：
    LICENSE_ADMIN_KEY  可替代 --admin-key
    LICENSE_SERVER_URL 可替代 --url（默认 http://127.0.0.1:8787）
"""
import argparse
import json
import os
import sys
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen


def issue(admin_key: str, server_url: str, lic_type: str, days: int | None, count: int):
    url = server_url.rstrip("/") + "/admin/issue"
    payload = json.dumps({
        "type": lic_type,
        "days": days,
        "count": count,
    }, ensure_ascii=False).encode("utf-8")

    req = Request(
        url,
        data=payload,
        headers={
            "Content-Type": "application/json",
            "X-Admin-Key": admin_key,
        },
        method="POST",
    )
    try:
        with urlopen(req, timeout=10) as resp:
            data = json.loads(resp.read().decode("utf-8"))
    except HTTPError as e:
        body = e.read().decode("utf-8")
        print(f"[错误] 服务器返回 {e.code}: {body}", file=sys.stderr)
        sys.exit(1)
    except URLError as e:
        print(f"[错误] 无法连接服务器: {e}", file=sys.stderr)
        sys.exit(1)

    if not data.get("success"):
        print(f"[错误] {data.get('error')}", file=sys.stderr)
        sys.exit(1)

    print("=" * 60)
    print(f"类型: {lic_type}   数量: {count}")
    for key in data["keys"]:
        print(key)
    print("=" * 60)


def main():
    parser = argparse.ArgumentParser(description="通过授权服务器签发激活码")
    parser.add_argument("--admin-key", default=os.environ.get("LICENSE_ADMIN_KEY"),
                        help="管理员密钥（或设置 LICENSE_ADMIN_KEY）")
    parser.add_argument("--url", default=os.environ.get("LICENSE_SERVER_URL", "http://127.0.0.1:8787"),
                        help="授权服务器地址")
    parser.add_argument("--type", choices=["month", "buyout"], default="month",
                        help="授权类型")
    parser.add_argument("--days", type=int, default=None,
                        help="月付天数（默认 30，buyout 忽略）")
    parser.add_argument("--count", type=int, default=1, help="签发数量")
    args = parser.parse_args()

    if not args.admin_key:
        raise SystemExit("缺少 --admin-key 或 LICENSE_ADMIN_KEY")

    days = args.days if args.type == "month" else None
    issue(args.admin_key, args.url, args.type, days, args.count)


if __name__ == "__main__":
    main()
