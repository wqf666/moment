# 贡献指南

感谢你对 **Moment - CUMT校园论坛** 项目的关注！我们欢迎所有形式的贡献。

## 🎯 如何贡献

### 1. 报告 Bug

如果你发现了 Bug，请创建一个 Issue，并包含以下信息：

- **问题描述**: 清晰简洁地描述问题
- **复现步骤**: 如何重现这个问题
- **预期行为**: 你期望发生什么
- **实际行为**: 实际发生了什么
- **环境信息**: 
  - 操作系统
  - 浏览器（前端问题）
  - 编译器版本（后端问题）
- **截图**: 如果适用，添加截图

### 2. 提出新功能建议

如果你想建议新功能：

- 先查看现有的 Issues，确保没有重复
- 创建一个新的 Issue，标签为 `enhancement`
- 详细描述功能需求和用例
- 说明这个功能为什么对项目有价值

### 3. 提交代码

#### 准备工作

1. Fork 本仓库
2. 克隆你的 Fork: `git clone https://github.com/your-username/moment-cumt.git`
3. 添加上游仓库: `git remote add upstream https://github.com/original-owner/moment-cumt.git`
4. 创建特性分支: `git checkout -b feature/your-feature-name`

#### 开发规范

**前端代码 (JavaScript)**
```javascript
// ✅ 好的命名
const userName = '张三'
function fetchPosts() { }

// ❌ 避免的命名
const n = '张三'
function f() { }
```

**后端代码 (C++)**
```cpp
// ✅ 好的命名
class UserController {
public:
    void handleLogin(const HttpRequestPtr& req);
private:
    std::string generateToken();
};

// 使用驼峰命名法
// 类名: PascalCase
// 函数/变量: camelCase
```

**CSS 样式**
```css
/* ✅ 使用 BEM 命名或语义化类名 */
.post-card { }
.post-card__title { }
.post-card--featured { }

/* 遵循紫色主题 */
.primary-btn {
  background: linear-gradient(135deg, #667eea, #764ba2);
}
```

#### 提交规范

使用清晰的 commit message：

```bash
# 格式: <type>: <description>

# 示例
git commit -m "feat: 添加用户头像上传功能"
git commit -m "fix: 修复AI助手消息滚动问题"
git commit -m "docs: 更新API文档"
git commit -m "style: 优化帖子卡片样式"
git commit -m "refactor: 重构用户认证逻辑"
```

类型包括：
- `feat`: 新功能
- `fix`: Bug修复
- `docs`: 文档更新
- `style`: 样式调整
- `refactor`: 代码重构
- `test`: 测试相关
- `chore`: 构建/工具链相关

#### 推送和PR

1. 推送到你的分支: `git push origin feature/your-feature-name`
2. 在 GitHub 上创建 Pull Request
3. 填写 PR 描述，说明：
   - 这个PR做了什么
   - 为什么需要这个改动
   - 如何测试这个改动
4. 等待代码审查

### 4. 改进文档

文档同样重要！你可以：

- 修正拼写或语法错误
- 补充不清晰的说明
- 添加示例代码
- 翻译文档
- 更新过时的信息

## 🔍 代码审查清单

提交 PR 前，请自查：

- [ ] 代码遵循项目规范
- [ ] 添加了必要的注释
- [ ] 更新了相关文档
- [ ] 测试通过（如果有测试）
- [ ] 没有留下调试代码
- [ ] Commit message 清晰明了
- [ ] 没有引入不必要的依赖
- [ ] 考虑了性能影响

## 💡 开发提示

### 前端开发

```bash
cd echo-client
npm install
npm run dev  # 开发模式
npm run build  # 生产构建
```

### 后端开发

```bash
cd echo-server/build
cmake .. -DCMAKE_BUILD_TYPE=Debug  # Debug模式便于调试
make -j$(nproc)
./echo_server
```

### AI服务开发

```bash
cd echo-server
bash scripts/start_ai.sh  # 启动AI服务
bash scripts/test_ai.sh   # 测试AI接口
```

## 🎨 设计规范

### 色彩规范

严格遵循紫色主题：
- 主色: `#667eea` → `#764ba2`
- 辅助色: `#f093fb`
- 背景渐变: `#667eea` → `#764ba2` → `#f093fb`

### Emoji 使用

在UI文案中适当使用emoji增强亲和力：
- 🏠 首页/广场
- 💫 动态/关注
- ✍️ 发布/编辑
- 👤 用户/个人
- ⚙️ 设置
- 🤖 AI助手
- 🎓 学校相关

### 响应式设计

确保新功能在移动端也能正常使用：
- 最小支持宽度: 320px
- 测试设备: iPhone SE, iPad, Desktop

## ❓ 常见问题

**Q: 我可以同时处理多个Issue吗？**  
A: 可以！但建议为每个功能创建独立的分支。

**Q: 我的PR多久会被审查？**  
A: 我们会尽快审查，通常在1-3个工作日内。

**Q: 如何联系维护者？**  
A: 可以通过Issue、Email或QQ群联系我们。

**Q: 新手可以贡献吗？**  
A: 当然可以！查看标记为 `good first issue` 的Issue开始。

## 🙏 致谢

感谢每一位为 Moment 项目做出贡献的开发者！

---

有任何问题，欢迎随时提出 Issue 或联系我们！
