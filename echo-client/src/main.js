import './style.css'

/*
  Echo Client V4
  功能：正式登录/注册界面 + 开发测试登录 + 社区基础功能。

  如果你的后端登录/注册接口不是下面这些名字，只需要改这里：
*/
const AUTH_LOGIN_URLS = ['/api/auth/login', '/api/login', '/api/users/login']
const AUTH_REGISTER_URLS = ['/api/auth/register', '/api/register', '/api/users/register']

const state = {
  token: localStorage.getItem('echo_token') || '',
  currentUserId: localStorage.getItem('echo_user_id') || '',
  currentUser: readJson('echo_user') || null,
  currentTab: localStorage.getItem('echo_tab') || 'latest',
  posts: [],
  detailPostId: null,
  uploadedImageUrl: '',
  viewedUserId: null,
  authMode: 'login',
  aiConversationId: 0,
  aiConversations: [],
  aiMessages: []
}

const app = document.querySelector('#app')

app.innerHTML = `
  <div class="app-shell">
    <header class="top-bar">
      <div>
        <h1>Echo 社区</h1>
        <p id="current-user-text">未登录</p>
      </div>
      <div class="top-actions">
        <button id="dev-login-open-btn" class="ghost-btn">测试登录</button>
        <button id="logout-btn" class="ghost-btn" style="display:none;">退出登录</button>
      </div>
    </header>

    <section id="auth-view" class="auth-grid">
      <div class="panel">
        <div class="auth-tabs">
          <button id="show-login-btn" class="auth-tab active">登录</button>
          <button id="show-register-btn" class="auth-tab">注册</button>
        </div>

        <div id="login-box">
          <h2>账号登录</h2>
          <p class="muted">正式模式：使用用户名和密码登录。</p>

          <div class="form-row">
            <label>用户名</label>
            <input id="login-username" class="input" placeholder="例如 alice" autocomplete="username" />
          </div>

          <div class="form-row">
            <label>密码</label>
            <input id="login-password" class="input" type="password" placeholder="请输入密码" autocomplete="current-password" />
          </div>

          <button id="login-btn" class="primary-btn big-btn">登录</button>
          <p id="login-tip" class="form-tip"></p>
        </div>

        <div id="register-box" style="display:none;">
          <h2>注册账号</h2>
          <p class="muted">注册成功后会自动尝试登录。</p>

          <div class="form-row">
            <label>用户名</label>
            <input id="register-username" class="input" placeholder="请输入用户名" autocomplete="username" />
          </div>

          <div class="form-row">
            <label>昵称</label>
            <input id="register-nickname" class="input" placeholder="可以不填" />
          </div>

          <div class="form-row">
            <label>密码</label>
            <input id="register-password" class="input" type="password" placeholder="至少 6 位更好" autocomplete="new-password" />
          </div>

          <button id="register-btn" class="primary-btn big-btn">注册</button>
          <p id="register-tip" class="form-tip"></p>
        </div>
      </div>

      <div id="dev-login-box" class="panel side-panel">
        <h2>开发测试登录</h2>
        <p class="muted">如果后端还没有正式登录接口，可以继续使用 demo_token。</p>
        <div class="form-row">
          <label>用户 ID</label>
          <input id="dev-user-id" class="input" value="1" placeholder="例如 1 或 2" />
        </div>
        <button id="dev-login-btn" class="ghost-btn big-btn">用 demo_token 登录</button>
        <div class="hint-box">
          <p>1 -> demo_token_1</p>
          <p>2 -> demo_token_2</p>
          <p>这个入口方便你继续开发前端。</p>
        </div>
      </div>
    </section>

    <section id="main-view" style="display:none;">
      <nav class="tabs">
        <button class="tab-btn" data-tab="latest">首页</button>
        <button class="tab-btn" data-tab="following">关注流</button>
        <button class="tab-btn" data-tab="create">发帖</button>
        <button class="tab-btn" data-tab="mine">我的主页</button>
        <button class="tab-btn" data-tab="profile">编辑资料</button>
        <button class="tab-btn" data-tab="ai">AI助手</button>
      </nav>

      <div id="status" class="status"></div>
      <div id="content"></div>
    </section>
  </div>

  <div id="modal" class="modal-mask" style="display:none;">
    <div class="modal-card">
      <button id="modal-close" class="modal-close">×</button>
      <div id="modal-content"></div>
    </div>
  </div>
`

const $ = (selector) => document.querySelector(selector)
const authView = $('#auth-view')
const mainView = $('#main-view')
const contentEl = $('#content')
const statusEl = $('#status')
const currentUserText = $('#current-user-text')
const logoutBtn = $('#logout-btn')
const devLoginOpenBtn = $('#dev-login-open-btn')
const modal = $('#modal')
const modalContent = $('#modal-content')

function readJson(key) {
  try {
    const value = localStorage.getItem(key)
    return value ? JSON.parse(value) : null
  } catch {
    return null
  }
}

function saveSession({ token, userId, user }) {
  state.token = token || ''
  state.currentUserId = String(userId || user?.user_id || user?.id || '')
  state.currentUser = user || null

  localStorage.setItem('echo_token', state.token)
  localStorage.setItem('echo_user_id', state.currentUserId)
  localStorage.setItem('echo_user', JSON.stringify(state.currentUser || {}))
}

function clearSession() {
  state.token = ''
  state.currentUserId = ''
  state.currentUser = null
  state.posts = []
  localStorage.removeItem('echo_token')
  localStorage.removeItem('echo_user_id')
  localStorage.removeItem('echo_user')
}

function escapeHtml(value) {
  return String(value ?? '')
    .replaceAll('&', '&amp;')
    .replaceAll('<', '&lt;')
    .replaceAll('>', '&gt;')
    .replaceAll('"', '&quot;')
    .replaceAll("'", '&#039;')
}

function setStatus(text = '') {
  statusEl.textContent = text
}

function normalizeApiUrl(url) {
  if (url.startsWith('http')) return url
  if (url.startsWith('/api/')) return url
  if (url === '/api') return url
  if (url.startsWith('/')) return `/api${url}`
  return `/api/${url}`
}

async function request(url, options = {}) {
  const headers = { ...(options.headers || {}) }

  if (!(options.body instanceof FormData)) {
    headers['Content-Type'] = 'application/json'
  }

  if (state.token) {
    headers.Authorization = `Bearer ${state.token}`
  }

  const res = await fetch(normalizeApiUrl(url), {
    ...options,
    headers
  })

  const text = await res.text()

  try {
    return JSON.parse(text)
  } catch {
    return {
      code: -1,
      message: `后端返回的不是 JSON：${text.slice(0, 160)}`
    }
  }
}

async function requestFirstAvailable(urls, options) {
  const errors = []

  for (const url of urls) {
    const result = await request(url, options)
    if (result.code === 0 || result.token || result.data?.token || result.data?.access_token) {
      return { result, url }
    }
    errors.push(`${url}: ${result.message || '失败'}`)
  }

  return {
    result: {
      code: -1,
      message: errors.join('；') || '没有可用接口'
    },
    url: ''
  }
}

function extractToken(result) {
  return (
    result.token ||
    result.access_token ||
    result.data?.token ||
    result.data?.access_token ||
    result.data?.jwt ||
    ''
  )
}

function extractUser(result, fallbackUsername = '') {
  return (
    result.user ||
    result.data?.user ||
    result.data?.profile ||
    result.data ||
    { username: fallbackUsername }
  )
}

function extractUserId(result, user) {
  return (
    result.user_id ||
    result.data?.user_id ||
    result.data?.id ||
    user?.user_id ||
    user?.id ||
    state.currentUserId ||
    ''
  )
}

function showAuth() {
  authView.style.display = 'grid'
  mainView.style.display = 'none'
  logoutBtn.style.display = 'none'
  devLoginOpenBtn.style.display = 'inline-block'
  currentUserText.textContent = '未登录'
}

function showMain() {
  authView.style.display = 'none'
  mainView.style.display = 'block'
  logoutBtn.style.display = 'inline-block'
  devLoginOpenBtn.style.display = 'none'
  updateCurrentUserText()
  syncTabs()
}

function updateCurrentUserText() {
  const user = state.currentUser || {}
  const name = user.nickname || user.username || (state.currentUserId ? `用户 ${state.currentUserId}` : '已登录')
  const userIdText = state.currentUserId ? ` · ID ${state.currentUserId}` : ''
  currentUserText.textContent = `当前登录：${name}${userIdText}`
}

function syncTabs() {
  document.querySelectorAll('.tab-btn').forEach((btn) => {
    btn.classList.toggle('active', btn.dataset.tab === state.currentTab)
  })
}

function setAuthMode(mode) {
  state.authMode = mode
  $('#login-box').style.display = mode === 'login' ? 'block' : 'none'
  $('#register-box').style.display = mode === 'register' ? 'block' : 'none'
  $('#show-login-btn').classList.toggle('active', mode === 'login')
  $('#show-register-btn').classList.toggle('active', mode === 'register')
}

async function loginWithPassword() {
  const username = $('#login-username').value.trim()
  const password = $('#login-password').value
  const tip = $('#login-tip')

  if (!username || !password) {
    tip.textContent = '请输入用户名和密码'
    return
  }

  tip.textContent = '正在登录...'

  const { result, url } = await requestFirstAvailable(AUTH_LOGIN_URLS, {
    method: 'POST',
    body: JSON.stringify({ username, password })
  })

  if (result.code !== 0 && !extractToken(result)) {
    tip.textContent = `登录失败：${result.message || '请检查后端登录接口'}。可以先用右侧测试登录。`
    return
  }

  const token = extractToken(result)
  const user = extractUser(result, username)
  const userId = extractUserId(result, user)

  if (!token) {
    tip.textContent = `登录接口 ${url} 没有返回 token，请检查后端返回字段。`
    return
  }

  saveSession({ token, userId, user })
  tip.textContent = ''
  showMain()
  await openTab('latest')
}

async function registerAccount() {
  const username = $('#register-username').value.trim()
  const nickname = $('#register-nickname').value.trim()
  const password = $('#register-password').value
  const tip = $('#register-tip')

  if (!username || !password) {
    tip.textContent = '请输入用户名和密码'
    return
  }

  tip.textContent = '正在注册...'

  const { result } = await requestFirstAvailable(AUTH_REGISTER_URLS, {
    method: 'POST',
    body: JSON.stringify({ username, nickname, password })
  })

  if (result.code !== 0) {
    tip.textContent = `注册失败：${result.message || '请检查后端注册接口'}`
    return
  }

  tip.textContent = '注册成功，正在自动登录...'
  $('#login-username').value = username
  $('#login-password').value = password
  setAuthMode('login')
  await loginWithPassword()
}

async function devLogin() {
  const userId = $('#dev-user-id').value.trim()
  if (!userId) {
    alert('请输入用户 ID')
    return
  }

  saveSession({
    token: `demo_token_${userId}`,
    userId,
    user: {
      user_id: Number(userId),
      username: `user_${userId}`,
      nickname: `测试用户 ${userId}`
    }
  })

  showMain()
  await openTab('latest')
}

function logout() {
  clearSession()
  contentEl.innerHTML = ''
  setStatus('')
  showAuth()
}

async function openTab(tab) {
  state.currentTab = tab
  localStorage.setItem('echo_tab', tab)
  syncTabs()
  setStatus('')

  if (tab === 'latest') return loadLatestPosts()
  if (tab === 'following') return loadFollowingFeed()
  if (tab === 'create') return renderCreatePost()
  if (tab === 'mine') return loadUserHome(state.currentUserId)
  if (tab === 'profile') return renderProfileEditor()
    if (tab === 'ai') return loadAiChat()
}

function getDisplayName(userOrPost) {
  return userOrPost?.nickname || userOrPost?.username || `用户 ${userOrPost?.user_id || ''}`
}

function getAvatarText(userOrPost) {
  return getDisplayName(userOrPost).slice(0, 1).toUpperCase()
}

function renderAvatar(userOrPost) {
  const avatarUrl = userOrPost?.avatar_url || ''
  const letter = escapeHtml(getAvatarText(userOrPost))

  if (!avatarUrl) return `<div class="avatar-text">${letter}</div>`

  return `
    <img class="avatar" src="${escapeHtml(avatarUrl)}" alt="头像"
      onerror="this.style.display='none'; this.nextElementSibling.style.display='flex';" />
    <div class="avatar-text" style="display:none;">${letter}</div>
  `
}

function renderPostImage(post) {
  if (!post.image_url) return ''
  return `<img class="post-image" src="${escapeHtml(post.image_url)}" alt="帖子图片" onerror="this.style.display='none';" />`
}

function renderPostCard(post) {
  const userId = post.user_id || ''
  const postId = post.post_id || post.id || ''

  return `
    <article class="post-card" data-post-id="${escapeHtml(postId)}">
      <div class="post-header">
        <button class="avatar-button user-link" data-user-id="${escapeHtml(userId)}">${renderAvatar(post)}</button>
        <div class="post-user">
          <button class="name-link user-link" data-user-id="${escapeHtml(userId)}">${escapeHtml(getDisplayName(post))}</button>
          <div class="meta">@${escapeHtml(post.username || '')} · 用户ID：${escapeHtml(userId)} · ${escapeHtml(post.created_at || '')}</div>
        </div>
      </div>

      <div class="post-content">${escapeHtml(post.content || '')}</div>
      ${renderPostImage(post)}

      <div class="actions">
        <button class="mini-btn like-btn" data-post-id="${escapeHtml(postId)}">
          ${post.liked ? '❤️' : '🤍'} <span>${escapeHtml(post.like_count || 0)}</span>
        </button>
        <button class="mini-btn detail-btn" data-post-id="${escapeHtml(postId)}">💬 ${escapeHtml(post.comment_count || 0)} 评论</button>
        ${post.is_owner ? `<button class="mini-btn edit-btn" data-post-id="${escapeHtml(postId)}">编辑</button>` : ''}
        ${post.is_owner ? `<button class="mini-btn danger delete-btn" data-post-id="${escapeHtml(postId)}">删除</button>` : ''}
        ${post.is_owner ? '<span class="tag mine-tag">我的帖子</span>' : '<span class="tag">别人的帖子</span>'}
      </div>
    </article>
  `
}

function renderPostList(posts, emptyText = '暂无帖子') {
  state.posts = posts || []
  if (!posts || posts.length === 0) {
    contentEl.innerHTML = `<div class="empty">${escapeHtml(emptyText)}</div>`
    return
  }
  contentEl.innerHTML = `<div class="post-list">${posts.map(renderPostCard).join('')}</div>`
}

async function loadLatestPosts() {
  setStatus('正在加载首页帖子...')
  const result = await request('/api/posts/latest?page=1&page_size=20')

  if (result.code !== 0) {
    setStatus(result.message || '加载失败')
    contentEl.innerHTML = ''
    return
  }

  setStatus('')
  renderPostList(result.data?.posts || [])
}

async function loadFollowingFeed() {
  setStatus('正在加载关注流...')
  const result = await request('/api/feed/following?page=1&page_size=20')

  if (result.code !== 0) {
    setStatus(result.message || '加载失败')
    contentEl.innerHTML = ''
    return
  }

  setStatus('')
  renderPostList(result.data?.posts || [], '关注的人还没有发帖')
}

function renderCreatePost() {
  state.uploadedImageUrl = ''
  contentEl.innerHTML = `
    <section class="panel">
      <h2>发布新帖子</h2>
      <div class="form-row">
        <label>内容</label>
        <textarea id="create-content" class="textarea" placeholder="写点什么..." rows="5"></textarea>
      </div>

      <div class="form-row">
        <label>图片 / 视频，可选</label>
        <input id="create-file" class="input" type="file" accept="image/*,video/*" />
        <button id="upload-create-file" class="ghost-btn">上传文件</button>
        <p id="create-upload-result" class="muted">还没有上传文件</p>
      </div>

      <div id="create-preview"></div>
      <button id="publish-btn" class="primary-btn big-btn">发布帖子</button>
    </section>
  `
}

async function uploadFile(file) {
  const formData = new FormData()
  formData.append('file', file)

  const result = await request('/api/upload/media', {
    method: 'POST',
    body: formData
  })

  if (result.code !== 0) throw new Error(result.message || '上传失败')

  return result.data?.media_url || result.data?.url || result.data?.file_url || result.media_url || ''
}

async function handleCreateUpload() {
  const file = $('#create-file')?.files?.[0]
  const resultText = $('#create-upload-result')
  const preview = $('#create-preview')

  if (!file) {
    alert('请先选择文件')
    return
  }

  try {
    resultText.textContent = '正在上传...'
    const url = await uploadFile(file)
    state.uploadedImageUrl = url
    resultText.textContent = url ? `上传成功：${url}` : '上传成功，但没有拿到 media_url'
    preview.innerHTML = url ? `<img class="preview-image" src="${escapeHtml(url)}" onerror="this.style.display='none';" />` : ''
  } catch (err) {
    resultText.textContent = err.message
  }
}

async function publishPost() {
  const content = $('#create-content').value.trim()

  if (!content && !state.uploadedImageUrl) {
    alert('内容和图片至少填一个')
    return
  }

  const result = await request('/api/posts/create', {
    method: 'POST',
    body: JSON.stringify({ content, image_url: state.uploadedImageUrl })
  })

  if (result.code !== 0) {
    alert(result.message || '发布失败')
    return
  }

  alert('发布成功')
  await openTab('latest')
}

async function toggleLike(postId, button) {
  button.disabled = true
  try {
    const result = await request('/api/posts/like/toggle', {
      method: 'POST',
      body: JSON.stringify({ post_id: Number(postId) })
    })

    if (result.code !== 0) {
      alert(result.message || '操作失败')
      return
    }

    if (modal.style.display === 'flex' && state.detailPostId) {
      await showPostDetail(state.detailPostId)
    }
    await openTab(state.currentTab)
  } finally {
    button.disabled = false
  }
}

async function deletePost(postId) {
  if (!confirm('确定删除这条帖子吗？')) return

  const result = await request('/api/posts/delete', {
    method: 'POST',
    body: JSON.stringify({ post_id: Number(postId) })
  })

  if (result.code !== 0) {
    alert(result.message || '删除失败')
    return
  }

  alert('删除成功')
  modal.style.display = 'none'
  await openTab(state.currentTab)
}

async function editPost(postId) {
  const post = state.posts.find((item) => String(item.post_id || item.id) === String(postId)) || {}
  const oldContent = post.content || ''
  const oldImageUrl = post.image_url || ''

  const newContent = prompt('请输入新的帖子内容：', oldContent)
  if (newContent === null) return

  const newImageUrl = prompt('请输入新的图片 URL，可以留空：', oldImageUrl)
  if (newImageUrl === null) return

  const result = await request('/api/posts/update', {
    method: 'POST',
    body: JSON.stringify({ post_id: Number(postId), content: newContent, image_url: newImageUrl })
  })

  if (result.code !== 0) {
    alert(result.message || '编辑失败')
    return
  }

  alert('编辑成功')
  await openTab(state.currentTab)
}

async function showPostDetail(postId) {
  modal.style.display = 'flex'
  modalContent.innerHTML = '<div class="status">正在加载帖子详情...</div>'
  state.detailPostId = postId

  const result = await request(`/api/posts/detail?post_id=${encodeURIComponent(postId)}`)

  if (result.code !== 0) {
    modalContent.innerHTML = `<div class="empty">${escapeHtml(result.message || '加载失败')}</div>`
    return
  }

  const data = result.data || {}
  const post = data.post || data.post_detail || data
  const comments = data.comments || data.comment_list || []

  modalContent.innerHTML = `
    <h2>帖子详情</h2>
    <div class="detail-post">${renderPostCard(post)}</div>

    <section class="comment-box">
      <h3>发表评论</h3>
      <textarea id="comment-content" class="textarea" rows="3" placeholder="写一条评论..."></textarea>
      <button id="send-comment-btn" class="primary-btn">发送评论</button>
    </section>

    <section class="comments">
      <h3>评论列表</h3>
      ${renderComments(comments)}
    </section>
  `
}

function renderComments(comments) {
  if (!comments || comments.length === 0) return '<div class="empty small-empty">暂无评论</div>'
  return `
    <div class="comment-list">
      ${comments.map((comment) => `
        <div class="comment-item">
          <button class="comment-author user-link" data-user-id="${escapeHtml(comment.user_id || '')}">
            ${escapeHtml(comment.nickname || comment.username || `用户 ${comment.user_id || ''}`)}
          </button>
          <span class="comment-time">${escapeHtml(comment.created_at || '')}</span>
          <div class="comment-content">${escapeHtml(comment.content || '')}</div>
        </div>
      `).join('')}
    </div>
  `
}

async function sendComment() {
  const content = $('#comment-content')?.value.trim()
  if (!content) {
    alert('请输入评论内容')
    return
  }

  const result = await request('/api/posts/comment', {
    method: 'POST',
    body: JSON.stringify({ post_id: Number(state.detailPostId), content })
  })

  if (result.code !== 0) {
    alert(result.message || '评论失败')
    return
  }

  await showPostDetail(state.detailPostId)
  await openTab(state.currentTab)
}

async function loadUserHome(userId) {
  if (!userId) {
    contentEl.innerHTML = '<div class="empty">没有用户 ID</div>'
    return
  }

  state.viewedUserId = String(userId)
  const isMe = String(userId) === String(state.currentUserId)

  setStatus(isMe ? '正在加载我的主页...' : '正在加载用户主页...')
  const result = await request(`/api/users/home?user_id=${encodeURIComponent(userId)}&page=1&page_size=20`)

  if (result.code !== 0) {
    setStatus(result.message || '加载失败')
    contentEl.innerHTML = ''
    return
  }

  setStatus('')
  const data = result.data || {}
  const user = data.user || {}
  const stats = data.stats || {}
  const posts = data.posts || []
  state.posts = posts

  if (isMe && user) {
    state.currentUser = user
    localStorage.setItem('echo_user', JSON.stringify(user))
    updateCurrentUserText()
  }

  contentEl.innerHTML = `
    <section class="profile-card">
      <div class="cover" style="background-image:url('${escapeHtml(user.cover_image_url || '')}')"></div>
      <div class="profile-main">
        <div class="avatar-wrap big-avatar">${renderAvatar(user)}</div>
        <div class="profile-info">
          <h2>${escapeHtml(getDisplayName(user))}</h2>
          <p class="muted">@${escapeHtml(user.username || '')} · 用户ID：${escapeHtml(user.user_id || userId)}</p>
          <p>${escapeHtml(user.bio || '这个人还没有简介')}</p>
        </div>
      </div>

      <div class="stats clickable-stats">
        <button class="stat-btn" data-list="posts">帖子 ${escapeHtml(stats.post_count || posts.length || 0)}</button>
        <button class="stat-btn" data-list="media">媒体 ${escapeHtml(stats.media_count || 0)}</button>
        <button class="stat-btn followers-btn" data-user-id="${escapeHtml(userId)}">粉丝 ${escapeHtml(stats.follower_count || 0)}</button>
        <button class="stat-btn following-btn" data-user-id="${escapeHtml(userId)}">关注 ${escapeHtml(stats.following_count || 0)}</button>
      </div>

      <div class="profile-actions">
        ${isMe ? '<button class="ghost-btn go-edit-profile-btn">编辑资料</button>' : renderFollowButton(user, userId)}
      </div>
    </section>

    <h2 class="section-title">${isMe ? '我的帖子' : 'TA 的帖子'}</h2>
    <div class="post-list">${posts.length ? posts.map(renderPostCard).join('') : '<div class="empty">还没有发过帖子</div>'}</div>
  `
}

function renderFollowButton(user, userId) {
  const following = Boolean(user.is_following)
  return `<button class="primary-btn follow-btn" data-user-id="${escapeHtml(userId)}">${following ? '取消关注' : '关注'}</button>`
}

async function toggleFollow(userId) {
  const result = await request('/api/follows/toggle', {
    method: 'POST',
    body: JSON.stringify({ target_user_id: Number(userId) })
  })

  if (result.code !== 0) {
    alert(result.message || '操作失败')
    return
  }

  await loadUserHome(userId)
}

async function showUserList(type, userId) {
  const title = type === 'followers' ? '粉丝列表' : '关注列表'
  modal.style.display = 'flex'
  modalContent.innerHTML = `<div class="status">正在加载${title}...</div>`

  const path = type === 'followers' ? '/api/follows/followers' : '/api/follows/following'
  const result = await request(`${path}?user_id=${encodeURIComponent(userId)}&page=1&page_size=50`)

  if (result.code !== 0) {
    modalContent.innerHTML = `<div class="empty">${escapeHtml(result.message || '加载失败')}</div>`
    return
  }

  const list = result.data?.followers || result.data?.following || result.data?.users || []

  modalContent.innerHTML = `
    <h2>${title}</h2>
    ${list.length ? `<div class="user-list">${list.map(renderUserListItem).join('')}</div>` : '<div class="empty">暂无用户</div>'}
  `
}

function renderUserListItem(user) {
  const userId = user.user_id || user.id || ''
  return `
    <div class="user-item">
      <button class="avatar-button user-link" data-user-id="${escapeHtml(userId)}">${renderAvatar(user)}</button>
      <div class="user-item-main">
        <button class="name-link user-link" data-user-id="${escapeHtml(userId)}">${escapeHtml(getDisplayName(user))}</button>
        <p class="muted">@${escapeHtml(user.username || '')} · 用户ID：${escapeHtml(userId)}</p>
        <p>${escapeHtml(user.bio || '')}</p>
      </div>
    </div>
  `
}

function renderProfileEditor() {
  const user = state.currentUser || {}
  contentEl.innerHTML = `
    <section class="panel">
      <h2>编辑资料</h2>
      <div class="form-row">
        <label>昵称</label>
        <input id="profile-nickname" class="input" value="${escapeHtml(user.nickname || '')}" placeholder="例如 EchoUser" />
      </div>

      <div class="form-row">
        <label>简介</label>
        <textarea id="profile-bio" class="textarea" rows="3" placeholder="写一句简介">${escapeHtml(user.bio || '')}</textarea>
      </div>

      <div class="form-row">
        <label>头像 URL</label>
        <input id="profile-avatar" class="input" value="${escapeHtml(user.avatar_url || '')}" placeholder="/api/media/file?name=xxx.jpg" />
      </div>

      <div class="form-row">
        <label>封面 URL</label>
        <input id="profile-cover" class="input" value="${escapeHtml(user.cover_image_url || '')}" placeholder="/api/media/file?name=xxx.jpg" />
      </div>

      <button id="save-profile-btn" class="primary-btn big-btn">保存资料</button>
    </section>
  `
}

async function saveProfile() {
  const payload = {
    nickname: $('#profile-nickname').value.trim(),
    bio: $('#profile-bio').value.trim(),
    avatar_url: $('#profile-avatar').value.trim(),
    cover_image_url: $('#profile-cover').value.trim()
  }

  const result = await request('/api/users/profile/update', {
    method: 'POST',
    body: JSON.stringify(payload)
  })

  if (result.code !== 0) {
    alert(result.message || '保存失败')
    return
  }

  alert('保存成功')
  await openTab('mine')
}
async function loadAiChat() {
  setStatus('正在加载 AI 助手...')

  const result = await request('/api/ai/conversations')

  if (result.code !== 0) {
    setStatus(result.message || '加载 AI 会话失败')
    contentEl.innerHTML = ''
    return
  }

  state.aiConversations = result.data?.conversations || []
  state.aiConversationId = state.aiConversations[0]?.conversation_id || 0

  if (state.aiConversationId) {
    await loadAiMessages(state.aiConversationId)
  } else {
    state.aiMessages = []
  }

  setStatus('')
  renderAiChat()
}

async function loadAiMessages(conversationId) {
  const result = await request(`/api/ai/messages?conversation_id=${encodeURIComponent(conversationId)}`)

  if (result.code !== 0) {
    alert(result.message || '加载聊天记录失败')
    return
  }

  state.aiConversationId = conversationId
  state.aiMessages = result.data?.messages || []
}

function renderAiChat() {
  contentEl.innerHTML = `
    <section class="ai-layout">
      <aside class="ai-sidebar">
        <div class="ai-sidebar-header">
          <h2>AI 会话</h2>
          <button id="new-ai-chat-btn" class="ghost-btn">新会话</button>
        </div>

        <div class="ai-conversation-list">
          ${
            state.aiConversations.length
              ? state.aiConversations.map((item) => `
                <button class="ai-conversation-item ${String(item.conversation_id) === String(state.aiConversationId) ? 'active' : ''}"
                  data-conversation-id="${escapeHtml(item.conversation_id)}">
                  <strong>${escapeHtml(item.title || 'AI Chat')}</strong>
                  <span>${escapeHtml(item.updated_at || '')}</span>
                </button>
              `).join('')
              : '<div class="empty small-empty">暂无会话</div>'
          }
        </div>
      </aside>

      <section class="ai-chat-panel">
        <div class="ai-message-list" id="ai-message-list">
          ${
            state.aiMessages.length
              ? state.aiMessages.map(renderAiMessage).join('')
              : '<div class="empty">开始和 AI 助手聊天吧</div>'
          }
        </div>

        <div class="ai-input-bar">
          <textarea id="ai-input" class="textarea" rows="3" placeholder="输入你想问 AI 的内容..."></textarea>
          <button id="send-ai-message-btn" class="primary-btn">发送</button>
        </div>
      </section>
    </section>
  `

  setTimeout(scrollAiToBottom, 0)
}

function renderAiMessage(message) {
  const role = message.role === 'assistant' ? 'assistant' : 'user'
  const name = role === 'assistant' ? 'AI助手' : '我'

  return `
    <div class="ai-message ${role}">
      <div class="ai-message-role">${name}</div>
      <div class="ai-message-content">${escapeHtml(message.content || '')}</div>
    </div>
  `
}

function scrollAiToBottom() {
  const box = $('#ai-message-list')
  if (box) box.scrollTop = box.scrollHeight
}


async function sendAiMessageStream() {
  const input = $('#ai-input')
  const btn = $('#send-ai-message-btn')
  const message = input.value.trim()
  if (!message) return

  input.value = ''
  btn.disabled = true
  btn.textContent = '发送中...'

  state.aiMessages.push({ role: 'user', content: message })
  renderAiChat()

  const response = await fetch('/api/ai/chat/stream', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json', Authorization: `Bearer ${state.token}` },
    body: JSON.stringify({ conversation_id: state.aiConversationId || 0, message })
  })

  const reader = response.body.getReader()
  const decoder = new TextDecoder()
  let done = false
  let assistantIndex = state.aiMessages.length
  state.aiMessages.push({ role: 'assistant', content: '' }) // 新消息占位

  while (!done) {
    const { value, done: readerDone } = await reader.read()
    done = readerDone
    if (value) {
      const text = decoder.decode(value)
      const lines = text.split('\n').filter((l) => l.startsWith('data: '))
      for (const line of lines) {
        const payload = line.replace(/^data: /, '').trim()
        if (payload === '[DONE]') {
          done = true
          break
        }
        try {
          const json = JSON.parse(payload)
          const contentDelta = json.choices?.[0]?.delta?.content
          if (contentDelta) {
            state.aiMessages[assistantIndex].content += contentDelta
            renderAiChat()
          }
        } catch (e) {
          // 非 JSON 直接忽略
        }
      }
    }
  }

  btn.disabled = false
  btn.textContent = '发送'
}




async function refreshAiConversations() {
  const result = await request('/api/ai/conversations')
  if (result.code === 0) {
    state.aiConversations = result.data?.conversations || []
  }
}
async function navigateToUser(userId) {
  if (!userId) return
  modal.style.display = 'none'
  state.currentTab = 'user'
  syncTabs()
  await loadUserHome(userId)
}

$('#show-login-btn').addEventListener('click', () => setAuthMode('login'))
$('#show-register-btn').addEventListener('click', () => setAuthMode('register'))
$('#login-btn').addEventListener('click', loginWithPassword)
$('#register-btn').addEventListener('click', registerAccount)
$('#dev-login-btn').addEventListener('click', devLogin)
$('#dev-login-open-btn').addEventListener('click', () => {
  $('#dev-login-box').scrollIntoView({ behavior: 'smooth', block: 'center' })
})
logoutBtn.addEventListener('click', logout)

$('#login-password').addEventListener('keydown', (event) => {
  if (event.key === 'Enter') loginWithPassword()
})
$('#register-password').addEventListener('keydown', (event) => {
  if (event.key === 'Enter') registerAccount()
})

$('#modal-close').addEventListener('click', () => { modal.style.display = 'none' })
modal.addEventListener('click', (event) => {
  if (event.target === modal) modal.style.display = 'none'
})

document.querySelector('.tabs').addEventListener('click', (event) => {
  const btn = event.target.closest('.tab-btn')
  if (btn) openTab(btn.dataset.tab)
})
contentEl.addEventListener('keydown', async (event) => {
  if (event.target?.id === 'ai-input' && event.key === 'Enter' && !event.shiftKey) {
    event.preventDefault()
    await sendAiMessageStream()
  }
})
contentEl.addEventListener('click', async (event) => {
  const target = event.target
  const userLink = target.closest('.user-link')
  const likeBtn = target.closest('.like-btn')
  const detailBtn = target.closest('.detail-btn')
  const deleteBtn = target.closest('.delete-btn')
  const editBtn = target.closest('.edit-btn')
  const followBtn = target.closest('.follow-btn')
  const followersBtn = target.closest('.followers-btn')
  const followingBtn = target.closest('.following-btn')

  if (userLink) return navigateToUser(userLink.dataset.userId)
  if (likeBtn) return toggleLike(likeBtn.dataset.postId, likeBtn)
  if (detailBtn) return showPostDetail(detailBtn.dataset.postId)
  if (deleteBtn) return deletePost(deleteBtn.dataset.postId)
  if (editBtn) return editPost(editBtn.dataset.postId)
  if (followBtn) return toggleFollow(followBtn.dataset.userId)
  if (followersBtn) return showUserList('followers', followersBtn.dataset.userId)
  if (followingBtn) return showUserList('following', followingBtn.dataset.userId)
  if (target.closest('.go-edit-profile-btn')) return openTab('profile')
  if (target.closest('#upload-create-file')) return handleCreateUpload()
  if (target.closest('#publish-btn')) return publishPost()
  if (target.closest('#save-profile-btn')) return saveProfile()
  if (target.closest('#new-ai-chat-btn')) {
  state.aiConversationId = 0
  state.aiMessages = []
  return renderAiChat()
}

  const aiConversationBtn = target.closest('.ai-conversation-item')
  if (aiConversationBtn) {
    await loadAiMessages(aiConversationBtn.dataset.conversationId)
    return renderAiChat()
  }

  if (target.closest('#send-ai-message-btn')) {
    return sendAiMessageStream()
  }
})

modalContent.addEventListener('click', async (event) => {
  const target = event.target
  const userLink = target.closest('.user-link')
  const sendBtn = target.closest('#send-comment-btn')
  const likeBtn = target.closest('.like-btn')
  const deleteBtn = target.closest('.delete-btn')
  const editBtn = target.closest('.edit-btn')

  if (userLink) return navigateToUser(userLink.dataset.userId)
  if (sendBtn) return sendComment()
  if (likeBtn) return toggleLike(likeBtn.dataset.postId, likeBtn)
  if (deleteBtn) return deletePost(deleteBtn.dataset.postId)
  if (editBtn) return editPost(editBtn.dataset.postId)
})

if (state.token) {
  showMain()
  openTab(state.currentTab === 'user' ? 'latest' : state.currentTab)
} else {
  showAuth()
}
