# GitHub上传前检查清单

在将Moment项目上传到GitHub之前，请确保完成以下检查：

## ✅ 必须完成的项目

### 1. 文件清理

- [ ] 删除大文件和模型文件（已在.gitignore中配置）
  - `echo-ai/models/` - AI模型文件
  - `echo-ai/llama.cpp.zip` - 压缩包
  - `node_modules/` - 前端依赖
  - `build/` - 编译产物
  - `uploads/` - 用户上传文件

- [ ] 删除临时文件
  ```bash
  rm -f echo-server/downloaded_test.jpg
  rm -f echo-server/0
  rm -f echo-server/app
  ```

### 2. 配置文件

- [x] `.gitignore` - 已创建完善的忽略规则
- [x] `.gitattributes` - 已配置行尾符处理
- [x] `LICENSE` - 已添加MIT许可证
- [x] `README.md` - 已创建详细的项目说明
- [x] `CONTRIBUTING.md` - 已创建贡献指南
- [x] `DEVELOPMENT.md` - 已创建开发文档

### 3. 代码质量

- [ ] 移除所有硬编码的密码和密钥
- [ ] 检查是否有调试代码残留
- [ ] 确保注释清晰准确
- [ ] 统一代码风格

#### 检查敏感信息

```bash
# 搜索可能的敏感信息
grep -r "password" --include="*.cpp" --include="*.js" .
grep -r "secret" --include="*.cpp" --include="*.js" .
grep -r "token" --include="*.cpp" --include="*.js" . | grep -v "demo_token"
```

### 4. 文档完善

- [x] README.md包含：
  - 项目介绍
  - 功能列表
  - 安装指南
  - 技术栈说明
  - 快速开始教程
  
- [x] CONTRIBUTING.md包含：
  - 如何报告Bug
  - 如何提交PR
  - 代码规范
  - Commit规范
  
- [x] DEVELOPMENT.md包含：
  - 架构说明
  - API文档
  - 数据库设计
  - 部署指南

### 5. 依赖管理

- [ ] 确认package.json中的依赖版本合理
- [ ] 确认CMakeLists.txt配置正确
- [ ] 考虑是否锁定依赖版本

### 6. 测试

- [ ] 后端API测试通过
  ```bash
  cd echo-server && bash test_api.sh
  ```

- [ ] AI接口测试通过（如果启用）
  ```bash
  cd echo-server && bash scripts/test_ai.sh
  ```

- [ ] 前端能正常启动
  ```bash
  cd echo-client && npm run dev
  ```

### 7. Git配置

- [ ] 配置Git用户信息
  ```bash
  git config user.name "Your Name"
  git config user.email "your.email@example.com"
  ```

- [ ] 检查.gitignore是否生效
  ```bash
  git status
  # 确认没有不应该追踪的文件
  ```

- [ ] 编写清晰的commit message
  ```bash
  git add .
  git commit -m "feat: initial commit - Moment CUMT校园论坛"
  ```

## 🔧 推荐优化项

### 1. 添加GitHub工作流（可选）

创建 `.github/workflows/ci.yml`:

```yaml
name: CI

on: [push, pull_request]

jobs:
  build-backend:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Install dependencies
        run: sudo apt-get install cmake g++ libssl-dev
      - name: Build
        run: |
          cd echo-server
          mkdir build && cd build
          cmake .. && make

  build-frontend:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - uses: actions/setup-node@v3
        with:
          node-version: '18'
      - name: Install and Build
        run: |
          cd echo-client
          npm install
          npm run build
```

### 2. 添加Issue模板

创建 `.github/ISSUE_TEMPLATE/bug_report.md`:

```markdown
---
name: Bug报告
about: 创建一个bug报告帮助我们改进
title: '[BUG] '
labels: bug
---

**描述问题**
清晰简洁地描述这个bug。

**复现步骤**
1. ...
2. ...
3. ...

**预期行为**
你期望发生什么？

**截图**
如果适用，添加截图。

**环境信息**
- OS: [e.g. Ubuntu 22.04]
- Browser: [e.g. Chrome 120]
- Version: [e.g. v1.0.0]
```

### 3. 添加Pull Request模板

创建 `.github/pull_request_template.md`:

```markdown
## 描述
这个PR做了什么改动？

## 类型
- [ ] Bug修复
- [ ] 新功能
- [ ] 文档更新
- [ ] 代码重构
- [ ] 其他

## 测试
- [ ] 我已测试过这些改动
- [ ] 添加了新的测试（如适用）

## 检查清单
- [ ] 我的代码遵循了项目的代码规范
- [ ] 我进行了自我审查
- [ ] 我更新了相关文档
- [ ] 没有新的警告产生
```

### 4. 添加徽章和状态

在README中添加：
- CI/CD状态徽章
- 代码覆盖率徽章
- License徽章
- 版本徽章

### 5. 截图和演示

- [ ] 添加应用截图到README
- [ ] 录制简短的演示视频（可选）
- [ ] 提供在线Demo链接（如果有）

### 6. 性能优化

- [ ] 检查前端打包体积
  ```bash
  cd echo-client && npm run build
  ls -lh dist/
  ```

- [ ] 优化图片资源
- [ ] 考虑使用CDN

### 7. 安全审查

- [ ] 检查是否有暴露的密钥
- [ ] 确认SQL查询使用参数化
- [ ] 验证输入过滤和转义
- [ ] 检查CORS配置

## 📋 最终检查

在推送之前，运行以下命令：

```bash
# 1. 检查Git状态
git status

# 2. 查看将要提交的文件
git diff --cached --stat

# 3. 确保没有大文件
git diff --cached --numstat | awk '$1 > 1000 {print $3}'

# 4. 最后一次构建测试
cd echo-client && npm run build && cd ..
cd echo-server/build && make && cd ../..

# 5. 提交
git add .
git commit -m "feat: prepare for GitHub release"

# 6. 推送到远程仓库
git remote add origin https://github.com/your-username/moment-cumt.git
git push -u origin main
```

## 🎉 上传后

1. 在GitHub上创建Release
2. 添加版本标签
   ```bash
   git tag -a v1.0.0 -m "Initial release"
   git push origin v1.0.0
   ```
3. 启用GitHub Pages（如果需要展示前端）
4. 配置Project看板
5. 邀请协作者
6. 分享到社区

## 📝 维护建议

- 定期更新依赖包
- 保持文档同步更新
- 及时响应Issues和PRs
- 定期发布新版本
- 收集用户反馈

---

完成以上检查后，你的项目就可以专业地展示在GitHub上了！🚀
