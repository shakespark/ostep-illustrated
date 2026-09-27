# OSTEP 图解：交互式学习站

《Operating Systems: Three Easy Pieces》40 个核心章节的中文交互学习材料。

在线阅读：<https://t.miaowuao.cn/ostep/>

## 本地打开

```bash
git clone <本仓库> && cd <目录>
python3 -m http.server 8765   # 浏览器访问 http://localhost:8765
```

不需要联网，也不需要构建。

## 部署

推送到 `main` 后，GitHub Actions（`.github/workflows/deploy.yml`）用 rsync 把站点同步到服务器的
`/var/www/tutorials/ostep/`，由 Nginx 提供访问。需要仓库 Secret `DEPLOY_SSH_KEY`（服务器上 `deploy` 用户的私钥）。

## 目录

| 路径 | 内容 |
|---|---|
| `index.html` | 总览：学习方法、概念地图、章节列表、学习进度 |
| `chapters/*.html` | 每章一页：导读、图解、交互模拟器、预测练习、自测题 |
| `labs/<章节>/` | C 实验，`make` 后运行，每个目录有 README |
| `source/` | 原书 PDF 与提取的文本，写作依据；版权归原作者，**不在仓库里**，从 [作者官网](https://pages.cs.wisc.edu/~remzi/OSTEP/) 获取 |
| `assets/` | 共享样式与脚本 |
| `AUTHORING.md` | 章节页编写规范（以后想补充或重写某章时用） |

学习进度保存在浏览器的 localStorage 中，只在本机本浏览器有效。
