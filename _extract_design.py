import zipfile, os
z = r'c:\Users\ADMIN\.trae-cn\attachments\6a682876d9dc9988b31d27f0\79f57db9-7f17-44f5-a557-55ab6d4b9cd5_e2bf3fcd-f6d7-4aa0-a449-5466b86ae713_主窗口 - 空白引导页 等 11 个设计.zip'
out = r'c:\Users\ADMIN\PycharmProjects\HistoryofBilibili\_design_extract'
os.makedirs(out, exist_ok=True)
with zipfile.ZipFile(z, 'r') as zf:
    for name in zf.namelist():
        zf.extract(name, out)
        print(name)
print('extracted to', out)
