document.addEventListener('DOMContentLoaded', function() {
    // 全局变量
    let messages = [];
    let currentUserName = null;
    let currentUserAvatar = null;
    let currentPage = 1;
    let pageSize = 10;
    let hasMore = true;
    let pendingReplies = {};
    
    // 初始化
    init();
    
    function init() {
        setupEventListeners();
        loadMessages(true);
        checkAuthStatus();
        setupDarkMode(); // 添加暗黑模式初始化
        
        // 每隔一段时间检查是否有新加载的留言可以匹配待处理回复
        setInterval(processPendingReplies, 5000);
    }
    
    // 设置事件监听器
    function setupEventListeners() {
        // 提交留言
        const submitMessageBtn = document.getElementById('submitMessage');
        if (submitMessageBtn) {
            submitMessageBtn.addEventListener('click', submitMessage);
        }
        
        // 加载更多
        const loadMoreBtn = document.getElementById('loadMoreBtn');
        if (loadMoreBtn) {
            loadMoreBtn.addEventListener('click', () => loadMessages(false));
        }
        
        // 添加用户头像点击事件
        const userAvatar = document.getElementById('userAvatar');
        const userDropdown = document.querySelector('.user-dropdown');
        
        if (userAvatar && userDropdown) {
            let timeoutId;
            
            // 点击头像显示下拉菜单
            userAvatar.addEventListener('click', function(e) {
                e.stopPropagation();
                userDropdown.classList.toggle('show');
            });
            
            // 鼠标离开头像时不立即隐藏，而是设置延时
            userAvatar.addEventListener('mouseout', function() {
                timeoutId = setTimeout(() => {
                    userDropdown.classList.remove('show');
                }, 300); // 300毫秒延迟
            });
            
            // 鼠标进入下拉菜单时，取消隐藏计时器
            userDropdown.addEventListener('mouseover', function() {
                clearTimeout(timeoutId);
            });
            
            // 鼠标离开下拉菜单时，设置延时隐藏
            userDropdown.addEventListener('mouseout', function() {
                timeoutId = setTimeout(() => {
                    userDropdown.classList.remove('show');
                }, 300); // 300毫秒延迟
            });
            
            // 点击页面其他区域关闭菜单
            document.addEventListener('click', function() {
                if (userDropdown.classList.contains('show')) {
                    userDropdown.classList.remove('show');
                }
            });
        }
        
        // 退出登录
        const logoutBtn = document.getElementById('logout');
        if (logoutBtn) {
            logoutBtn.addEventListener('click', function(e) {
                e.preventDefault();
                logout();
            });
        }
        
        // 回到顶部按钮
        const backToTopBtn = document.getElementById('backToTop');
        if (backToTopBtn) {
            window.addEventListener('scroll', function() {
                if (window.pageYOffset > 300) {
                    backToTopBtn.classList.add('visible');
                } else {
                    backToTopBtn.classList.remove('visible');
                }
            });
            
            backToTopBtn.addEventListener('click', function() {
                window.scrollTo({
                    top: 0,
                    behavior: 'smooth'
                });
            });
        }
        
        // 主题切换
        const themeToggle = document.getElementById('themeToggle');
        if (themeToggle) {
            themeToggle.addEventListener('click', function() {
                toggleDarkMode();
            });
        }
    }
    
    // 检查登录状态
    function checkAuthStatus() {
        fetch('/api/user/info')
            .then(response => {
                if (!response.ok) throw new Error('未登录');
                return response.json();
            })
            .then(data => {
                currentUserName = data.username;
                currentUserAvatar = data.avatar;
                updateUIForAuthStatus(true);
            })
            .catch(() => {
                // 使用模拟数据
                currentUserName = '游客';
                currentUserAvatar = 'img/default_touxiang.jpg';
                updateUIForAuthStatus(false);
            });
    }
    
    // 更新UI显示登录状态
    function updateUIForAuthStatus(isLoggedIn) {
        const currentUserAvatarEl = document.getElementById('currentUserAvatar');
        const userAvatar = document.getElementById('userAvatar');
        const userDropdown = document.querySelector('.user-dropdown');
        
        if (currentUserAvatarEl) {
            currentUserAvatarEl.src = currentUserAvatar;
        }
        
        if (userAvatar) {
            userAvatar.src = currentUserAvatar;
        }
        
        if (isLoggedIn) {
            // 用户已登录，更新下拉菜单
            if (userDropdown) {
                userDropdown.innerHTML = `
                    <a href="user_center.html"><i class="fas fa-user-circle"></i> 个人中心</a>
                    <a href="blog_editor.html"><i class="fas fa-edit"></i> 写博客</a>
                    <a href="#" id="logout"><i class="fas fa-sign-out-alt"></i> 退出登录</a>
                `;
                
                // 重新绑定退出登录事件
                const logoutBtn = document.getElementById('logout');
                if (logoutBtn) {
                    logoutBtn.addEventListener('click', function(e) {
                        e.preventDefault();
                        logout();
                    });
                }
            }
        } else {
            // 用户未登录，显示游客信息
            if (userDropdown) {
                userDropdown.innerHTML = `
                    <a href="blog_login.html"><i class="fas fa-sign-in-alt"></i> 登录</a>
                    <a href="blog_register.html"><i class="fas fa-user-plus"></i> 注册</a>
                `;
            }
        }
    }
    
    // 加载留言
    function loadMessages(isFirstLoad) {
        const loadMoreBtn = document.getElementById('loadMoreBtn');
        if (loadMoreBtn) {
            loadMoreBtn.disabled = true;
            loadMoreBtn.innerHTML = '<i class="fas fa-spinner fa-spin"></i> 加载中...';
        }
        
        if (isFirstLoad) {
            currentPage = 1;
            messages = [];
        }
        
        fetch(`/api/messages_board?page=${currentPage}&size=${pageSize}`)
            .then(response => {
                if (!response.ok) throw new Error('网络响应异常');
                return response.json();
            })
            .then(data => {
                // 获取所有留言和回复
                const allMessages = data.messages || [];
                hasMore = data.hasMore || false;
                
                // 将留言和回复进行分类
                const parentMessages = [];
                const newChildMessages = {};
                
                allMessages.forEach(msg => {
                    if (msg.parentId === -1 || msg.parentId === null) {
                        // 顶级留言
                        msg.replies = [];
                        parentMessages.push(msg);
                    } else {
                        // 回复留言
                        if (!newChildMessages[msg.parentId]) {
                            newChildMessages[msg.parentId] = [];
                        }
                        newChildMessages[msg.parentId].push(msg);
                    }
                });
                
                // 将回复分配给相应的父留言（当前已加载的留言）
                parentMessages.forEach(parent => {
                    // 检查当前回复列表
                    if (newChildMessages[parent.id]) {
                        parent.replies = newChildMessages[parent.id];
                        parent.replyCount = parent.replies.length;
                        
                        // 从newChildMessages中移除已处理的回复
                        delete newChildMessages[parent.id];
                    }
                    
                    // 检查是否有待处理的回复
                    if (pendingReplies[parent.id]) {
                        if (!parent.replies) parent.replies = [];
                        // 将待处理回复添加到父留言中
                        parent.replies = [...parent.replies, ...pendingReplies[parent.id]];
                        parent.replyCount = parent.replies.length;
                        
                        // 删除已处理的待处理回复
                        delete pendingReplies[parent.id];
                    }
                });
                
                // 处理当前页面的留言中与已加载留言有关的回复
                if (!isFirstLoad) {
                    // 检查这些回复是否属于已加载的留言
                    Object.keys(newChildMessages).forEach(parentId => {
                        const existingParentIndex = messages.findIndex(m => m.id == parentId);
                        if (existingParentIndex !== -1) {
                            // 找到了父留言，更新它的回复列表
                            const parent = messages[existingParentIndex];
                            if (!parent.replies) parent.replies = [];
                            parent.replies = [...parent.replies, ...newChildMessages[parentId]];
                            parent.replyCount = parent.replies.length;
                            
                            // 更新DOM中相应元素的回复计数
                            updateReplyCount(parentId, parent.replyCount);
                            
                            // 如果回复区域已打开，则更新回复内容
                            updateRepliesIfVisible(parentId, parent.replies);
                            
                            // 已处理的回复从newChildMessages中移除
                            delete newChildMessages[parentId];
                        }
                    });
                }
                
                // 剩余的回复（孤儿回复）放入待处理缓存
                Object.keys(newChildMessages).forEach(parentId => {
                    if (!pendingReplies[parentId]) {
                        pendingReplies[parentId] = [];
                    }
                    pendingReplies[parentId] = [...pendingReplies[parentId], ...newChildMessages[parentId]];
                });
                
                // 按时间倒序排序顶级留言
                parentMessages.sort((a, b) => new Date(b.date) - new Date(a.date));
                
                if (isFirstLoad) {
                    messages = parentMessages;
                } else {
                    const newMessages = parentMessages;
                    renderNewMessages(newMessages); // 只渲染新加载的消息
                    messages = [...messages, ...newMessages];
                    currentPage++;
                    
                    // 更新加载更多按钮状态
                    if (loadMoreBtn) {
                        loadMoreBtn.disabled = false;
                        loadMoreBtn.innerHTML = '<i class="fas fa-spinner"></i> 加载更多';
                        loadMoreBtn.style.display = hasMore ? 'block' : 'none';
                    }
                    return; // 提前返回，避免执行下面的完整渲染
                }
                
                renderMessages(isFirstLoad);
                currentPage++;
                
                // 更新加载更多按钮状态
                if (loadMoreBtn) {
                    loadMoreBtn.disabled = false;
                    loadMoreBtn.innerHTML = '<i class="fas fa-spinner"></i> 加载更多';
                    loadMoreBtn.style.display = hasMore ? 'block' : 'none';
                }
            })
            .catch(error => {
                console.error('获取留言列表失败:', error);
                // 使用模拟数据
                const mockData = generateMockMessagesWithReplies(pageSize);
                hasMore = currentPage < 3; // 模拟数据最多加载3页
                
                if (isFirstLoad) {
                    messages = mockData;
                    renderMessages(true);
                } else {
                    renderNewMessages(mockData); // 只渲染新加载的消息
                    messages = [...messages, ...mockData];
                }
                
                currentPage++;
                
                // 更新加载更多按钮状态
                if (loadMoreBtn) {
                    loadMoreBtn.disabled = false;
                    loadMoreBtn.innerHTML = '<i class="fas fa-spinner"></i> 加载更多';
                    loadMoreBtn.style.display = hasMore ? 'block' : 'none';
                }
                
                showNotification('使用模拟数据显示', 'info');
            });
    }
    
    // 新增函数：只渲染新加载的消息
    function renderNewMessages(newMessages) {
        const messagesContainer = document.getElementById('messagesContainer');
        if (!messagesContainer) return;
        
        let html = '';
        newMessages.forEach(message => {
            const isOwnMessage = message.author === currentUserName;
            
            html += `
            <div class="message-item" id="message-${message.id}">
                <div class="message-header">
                    <div class="message-author">
                        <img src="${escapeHtml(message.authorAvatar)}" alt="${escapeHtml(message.author)}" class="message-author-avatar">
                        <div class="message-author-info">
                            <div class="message-author-name">${escapeHtml(message.author)}</div>
                            <div class="message-date">${formatDate(message.date)}</div>
                        </div>
                    </div>
                    ${isOwnMessage ? `
                        <button class="delete-btn" onclick="deleteMessage(${message.id})">
                            <i class="fas fa-trash"></i>
                        </button>
                    ` : ''}
                </div>
                <div class="message-content">${escapeHtml(message.content)}</div>
                <div class="message-actions">
                    <button class="action-btn ${message.isLiked ? 'liked' : ''}" onclick="toggleLike(${message.id})">
                        <i class="fa${message.isLiked ? 's' : 'r'} fa-heart"></i>
                        <span class="like-count">${message.likes || 0}</span>
                    </button>
                    <button class="action-btn reply-toggle" onclick="toggleReplies(${message.id})">
                        <i class="far fa-comment"></i>
                        <span class="reply-count">${message.replyCount || 0}</span>
                    </button>
                </div>
                <div class="replies-section" id="replies-${message.id}" style="display: none;">
                    <div class="replies-container">
                        ${renderRepliesHTML(message.replies || [])}
                    </div>
                    <div class="reply-form">
                        <textarea placeholder="写下你的回复..."></textarea>
                        <div class="reply-form-actions">
                            <button class="cancel-btn" onclick="hideReplies(${message.id})">取消</button>
                            <button class="submit-btn" onclick="submitReply(${message.id})">发表回复</button>
                        </div>
                    </div>
                </div>
            </div>
            `;
        });
        
        messagesContainer.insertAdjacentHTML('beforeend', html);
    }
    
    // 生成带有回复的模拟留言数据
    function generateMockMessagesWithReplies(count) {
        const mockMessages = [];
        const avatars = ['img/default_touxiang.jpg'];
        const names = ['张三', '李四', '王五', '赵六', '游客'];
        const contents = [
            '这是一条测试留言',
            '留言板功能很好用',
            '希望能添加更多功能',
            '期待后续更新',
            '支持一下作者'
        ];
        
        for (let i = 0; i < count; i++) {
            const messageId = Date.now() + i;
            const replyCount = Math.floor(Math.random() * 5);
            const replies = [];
            
            // 为每条留言生成随机数量的回复
            for (let j = 0; j < replyCount; j++) {
                replies.push({
                    id: messageId * 100 + j,
                    parentId: messageId,  // 回复引用对应的留言ID
                    author: names[Math.floor(Math.random() * names.length)],
                    authorAvatar: avatars[0],
                    content: `这是对留言的第${j + 1}条回复`,
                    date: new Date(Date.now() - j * 3600000).toISOString(),
                    likes: Math.floor(Math.random() * 10),
                    isLiked: false
                });
            }
            
            mockMessages.push({
                id: messageId,
                parentId: -1,  // 设置顶级留言的parentId为-1
                author: names[Math.floor(Math.random() * names.length)],
                authorAvatar: avatars[0],
                content: contents[Math.floor(Math.random() * contents.length)],
                date: new Date(Date.now() - Math.random() * 10000000000).toISOString(),
                likes: Math.floor(Math.random() * 50),
                isLiked: false,
                replyCount: replyCount,
                replies: replies
            });
        }
        return mockMessages;
    }
    
    // 添加HTML转义函数
    function escapeHtml(unsafe) {
        if (!unsafe) return '';
        return unsafe
            .toString()
            .replace(/&/g, "&amp;")
            .replace(/</g, "&lt;")
            .replace(/>/g, "&gt;")
            .replace(/"/g, "&quot;")
            .replace(/'/g, "&#039;");
    }
    
    // 修改渲染留言的函数，对留言内容进行转义
    function renderMessages(isFirstLoad) {
        const messagesContainer = document.getElementById('messagesContainer');
        if (!messagesContainer) return;
        
        if (messages.length === 0 && isFirstLoad) {
            messagesContainer.innerHTML = '<div class="no-messages">暂无留言</div>';
            return;
        }
        
        let html = '';
        messages.forEach(message => {
            const isOwnMessage = message.author === currentUserName;
            
            html += `
            <div class="message-item" id="message-${message.id}">
                <div class="message-header">
                    <div class="message-author">
                        <img src="${escapeHtml(message.authorAvatar)}" alt="${escapeHtml(message.author)}" class="message-author-avatar">
                        <div class="message-author-info">
                            <div class="message-author-name">${escapeHtml(message.author)}</div>
                            <div class="message-date">${formatDate(message.date)}</div>
                        </div>
                    </div>
                    ${isOwnMessage ? `
                        <button class="delete-btn" onclick="deleteMessage(${message.id})">
                            <i class="fas fa-trash"></i>
                        </button>
                    ` : ''}
                </div>
                <div class="message-content">${escapeHtml(message.content)}</div>
                <div class="message-actions">
                    <button class="action-btn ${message.isLiked ? 'liked' : ''}" onclick="toggleLike(${message.id})">
                        <i class="fa${message.isLiked ? 's' : 'r'} fa-heart"></i>
                        <span class="like-count">${message.likes || 0}</span>
                    </button>
                    <button class="action-btn reply-toggle" onclick="toggleReplies(${message.id})">
                        <i class="far fa-comment"></i>
                        <span class="reply-count">${message.replyCount || 0}</span>
                    </button>
                </div>
                <div class="replies-section" id="replies-${message.id}" style="display: none;">
                    <div class="replies-container">
                        ${renderRepliesHTML(message.replies || [])}
                    </div>
                    <div class="reply-form">
                        <textarea placeholder="写下你的回复..."></textarea>
                        <div class="reply-form-actions">
                            <button class="cancel-btn" onclick="hideReplies(${message.id})">取消</button>
                            <button class="submit-btn" onclick="submitReply(${message.id})">发表回复</button>
                        </div>
                    </div>
                </div>
            </div>
            `;
        });
        
        if (isFirstLoad) {
            messagesContainer.innerHTML = html;
        } else {
            messagesContainer.insertAdjacentHTML('beforeend', html);
        }
    }
    
    // 修改渲染回复的函数，添加删除按钮
    function renderRepliesHTML(replies) {
        if (!replies || replies.length === 0) {
            return '<div class="no-replies">暂无回复</div>';
        }
        
        let html = '';
        replies.forEach(reply => {
            const isOwnReply = reply.author === currentUserName;
            
            html += `
            <div class="reply-item" id="reply-${reply.id}">
                <div class="reply-author">
                    <img src="${escapeHtml(reply.authorAvatar)}" alt="${escapeHtml(reply.author)}" class="reply-avatar">
                    <div class="reply-info">
                        <div class="reply-name">${escapeHtml(reply.author)}</div>
                        <div class="reply-date">${formatDate(reply.date)}</div>
                    </div>
                    ${isOwnReply ? `
                        <button class="delete-btn" onclick="deleteReply(${reply.parentId}, ${reply.id})">
                            <i class="fas fa-trash"></i>
                        </button>
                    ` : ''}
                </div>
                <div class="reply-content">${escapeHtml(reply.content)}</div>
                <div class="reply-actions">
                    <button class="action-btn ${reply.isLiked ? 'liked' : ''}" onclick="toggleReplyLike(${reply.parentId}, ${reply.id})">
                        <i class="fa${reply.isLiked ? 's' : 'r'} fa-heart"></i>
                        <span class="like-count">${reply.likes || 0}</span>
                    </button>
                </div>
            </div>
            `;
        });
        
        return html;
    }
    
    // 切换显示回复区域
    function toggleReplies(messageId) {
        const repliesSection = document.getElementById(`replies-${messageId}`);
        
        if (repliesSection.style.display === 'none') {
            repliesSection.style.display = 'block';
            // 移除自动聚焦回复框的功能
            // 这样用户的视角不会自动移到回复框
        } else {
            repliesSection.style.display = 'none';
        }
    }
    
    // 隐藏回复区域
    function hideReplies(messageId) {
        const repliesSection = document.getElementById(`replies-${messageId}`);
        repliesSection.style.display = 'none';
    }
    
    // 提交回复
    function submitReply(messageId) {
        if (!currentUserName || currentUserName === '游客') {
            showLoginNotification('请先登录后才能发表回复');
            return;
        }
        
        const repliesSection = document.getElementById(`replies-${messageId}`);
        const textarea = repliesSection.querySelector('textarea');
        const content = textarea.value.trim();
        
        if (!content) {
            showNotification('回复内容不能为空', 'error');
            return;
        }
        
        fetch(`/api/messages_board/replies/${messageId}`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ 
                content, 
                parentId: messageId  // 设置回复的parentId为所回复留言的ID
            })
        })
        .then(response => {
            if (!response.ok) throw new Error('发表回复失败');
            return response.json();
        })
        .then(data => {
            // 添加回复到对应的留言中
            const message = messages.find(m => m.id === messageId);
            if (message) {
                if (!message.replies) message.replies = [];
                message.replies.push(data.reply);
                message.replyCount = message.replies.length;
                
                // 更新回复区域内容
                const repliesContainer = repliesSection.querySelector('.replies-container');
                repliesContainer.innerHTML = renderRepliesHTML(message.replies);
                
                // 更新回复数量显示
                updateReplyCount(messageId, message.replyCount);
                
                // 清空回复框
                textarea.value = '';
                showNotification('回复发表成功', 'success');
            }
        })
        .catch(() => {
            // 使用模拟数据
            const message = messages.find(m => m.id === messageId);
            if (message) {
                if (!message.replies) message.replies = [];
                
                const mockReply = {
                    id: Date.now(),
                    parentId: messageId,
                    author: currentUserName,
                    authorAvatar: currentUserAvatar,
                    content: content,
                    date: new Date().toISOString(),
                    likes: 0,
                    isLiked: false
                };
                
                message.replies.push(mockReply);
                message.replyCount = message.replies.length;
                
                // 更新回复区域内容
                const repliesContainer = repliesSection.querySelector('.replies-container');
                repliesContainer.innerHTML = renderRepliesHTML(message.replies);
                
                // 更新回复数量显示
                updateReplyCount(messageId, message.replyCount);
                
                // 清空回复框
                textarea.value = '';
                showNotification('回复发表成功（模拟数据）', 'success');
            }
        });
    }
    
    // 更新回复数量显示
    function updateReplyCount(messageId, count) {
        const replyCountEl = document.querySelector(`#message-${messageId} .reply-count`);
        if (replyCountEl) {
            replyCountEl.textContent = count;
        }
    }
    
    // 对回复点赞
    function toggleReplyLike(messageId, replyId) {
        if (!currentUserName || currentUserName === '游客') {
            showLoginNotification('请先登录后才能点赞回复');
            return;
        }
        
        const message = messages.find(m => m.id === messageId);
        if (!message || !message.replies) return;
        
        const reply = message.replies.find(r => r.id === replyId);
        if (!reply) return;
        
        const newLikeStatus = !reply.isLiked;
        
        fetch(`/api/messages_board/replies/like/${replyId}`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ liked: newLikeStatus })
        })
        .then(response => {
            if (!response.ok) throw new Error('操作失败');
            return response.json();
        })
        .then(data => {
            reply.likes = data.likes;
            reply.isLiked = data.isLiked;
            updateReplyLikeUI(replyId, data.likes, data.isLiked);
        })
        .catch(() => {
            // 使用模拟数据
            reply.isLiked = newLikeStatus;
            reply.likes = (reply.likes || 0) + (newLikeStatus ? 1 : -1);
            updateReplyLikeUI(replyId, reply.likes, reply.isLiked);
        });
    }
    
    // 将所有需要从 HTML 调用的函数添加到 window 对象
    window.toggleLike = toggleLike;
    window.toggleReplies = toggleReplies;
    window.hideReplies = hideReplies;
    window.submitReply = submitReply;
    window.deleteMessage = deleteMessage;
    window.toggleReplyLike = toggleReplyLike;
    window.deleteReply = deleteReply;
    
    // 格式化日期
    function formatDate(dateString) {
        const date = new Date(dateString);
        const now = new Date();
        const diff = now - date;
        
        if (diff < 60000) { // 小于1分钟
            return '刚刚';
        } else if (diff < 3600000) { // 小于1小时
            return `${Math.floor(diff / 60000)}分钟前`;
        } else if (diff < 86400000) { // 小于1天
            return `${Math.floor(diff / 3600000)}小时前`;
        } else {
            return `${date.getFullYear()}-${String(date.getMonth() + 1).padStart(2, '0')}-${String(date.getDate()).padStart(2, '0')}`;
        }
    }
    
    // 格式化留言内容
    function formatMessageContent(content) {
        if (!content) return '';
        return content.replace(/\n/g, '<br>');
    }
    
    // 提交留言
    function submitMessage() {
        const messageInput = document.getElementById('messageInput');
        if (!messageInput || !messageInput.value.trim()) {
            showNotification('留言内容不能为空', 'error');
            return;
        }
        
        // 检查登录状态
        if (!currentUserName || currentUserName === '游客') {
            showLoginNotification('请先登录后才能发布留言');
            return;
        }
        
        const content = messageInput.value.trim();
        
        fetch('/api/messages_board', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ 
                content,
                parentId: -1
            })
        })
        .then(response => {
            if (!response.ok) throw new Error('发布失败');
            return response.json();
        })
        .then(data => {
            messageInput.value = '';
            // 确保新留言有正确的结构
            const newMessage = {
                ...data.message,
                parentId: -1,
                replies: [],
                replyCount: 0
            };
            messages.unshift(newMessage);
            
            // 仅添加新的留言到容器顶部，而不是重新渲染整个列表
            const messagesContainer = document.getElementById('messagesContainer');
            if (messagesContainer) {
                // 创建新留言的HTML
                let newMessageHtml = createMessageHTML(newMessage);
                // 将新留言添加到容器顶部
                messagesContainer.insertAdjacentHTML('afterbegin', newMessageHtml);
            }
            
            showNotification('留言发表成功', 'success');
        })
        .catch(() => {
            // 使用模拟数据
            const mockMessage = {
                id: Date.now(),
                author: currentUserName,
                authorAvatar: currentUserAvatar,
                content: content,
                date: new Date().toISOString(),
                parentId: -1,
                replies: [],
                replyCount: 0,
                likes: 0,
                isLiked: false
            };
            messages.unshift(mockMessage);
            messageInput.value = '';
            
            // 直接添加新留言到容器顶部而不是重新渲染
            const messagesContainer = document.getElementById('messagesContainer');
            if (messagesContainer) {
                messagesContainer.insertAdjacentHTML('afterbegin', createMessageHTML(mockMessage));
            }
            
            showNotification('留言发表成功（模拟数据）', 'success');
        });
    }
    
    // 新增：创建单条留言HTML的辅助函数
    function createMessageHTML(message) {
        const isOwnMessage = message.author === currentUserName;
        
        return `
        <div class="message-item" id="message-${message.id}">
            <div class="message-header">
                <div class="message-author">
                    <img src="${escapeHtml(message.authorAvatar)}" alt="${escapeHtml(message.author)}" class="message-author-avatar">
                    <div class="message-author-info">
                        <div class="message-author-name">${escapeHtml(message.author)}</div>
                        <div class="message-date">${formatDate(message.date)}</div>
                    </div>
                </div>
                ${isOwnMessage ? `
                    <button class="delete-btn" onclick="deleteMessage(${message.id})">
                        <i class="fas fa-trash"></i>
                    </button>
                ` : ''}
            </div>
            <div class="message-content">${escapeHtml(message.content)}</div>
            <div class="message-actions">
                <button class="action-btn ${message.isLiked ? 'liked' : ''}" onclick="toggleLike(${message.id})">
                    <i class="fa${message.isLiked ? 's' : 'r'} fa-heart"></i>
                    <span class="like-count">${message.likes || 0}</span>
                </button>
                <button class="action-btn reply-toggle" onclick="toggleReplies(${message.id})">
                    <i class="far fa-comment"></i>
                    <span class="reply-count">${message.replyCount || 0}</span>
                </button>
            </div>
            <div class="replies-section" id="replies-${message.id}" style="display: none;">
                <div class="replies-container">
                    <div class="no-replies">暂无回复</div>
                </div>
                <div class="reply-form">
                    <textarea placeholder="写下你的回复..."></textarea>
                    <div class="reply-form-actions">
                        <button class="cancel-btn" onclick="hideReplies(${message.id})">取消</button>
                        <button class="submit-btn" onclick="submitReply(${message.id})">发表回复</button>
                    </div>
                </div>
            </div>
        </div>
        `;
    }
    
    // 替换原有的showLoginNotification函数为更美观的实现
    function showLoginNotification(message) {
        // 先移除可能存在的其他登录提示
        const existingNotification = document.querySelector('.login-notification');
        if (existingNotification) {
            existingNotification.remove();
        }
        
        // 创建更美观的通知元素
        const notification = document.createElement('div');
        notification.className = 'notification login-notification with-button';
        notification.innerHTML = `
            <div class="notification-content">
                <i class="fas fa-user-lock"></i>
                <span>${message}</span>
            </div>
            <a href="blog_login.html" class="notification-login-btn">立即登录</a>
            <button class="close-btn"><i class="fas fa-times"></i></button>
        `;
        
        document.body.appendChild(notification);
        
        // 添加动画效果
        setTimeout(() => {
            notification.classList.add('show');
        }, 10);
        
        // 添加关闭按钮事件
        const closeBtn = notification.querySelector('.close-btn');
        closeBtn.addEventListener('click', () => {
            notification.classList.remove('show');
            setTimeout(() => {
                notification.remove();
            }, 300);
        });
        
        // 5秒后自动关闭
        setTimeout(() => {
            if (notification.parentNode) {
                notification.classList.remove('show');
                setTimeout(() => {
                    notification.remove();
                }, 300);
            }
        }, 5000);
    }
    
    // 添加创建确认对话框的函数
    function createConfirmDialog(message, confirmCallback) {
        // 移除可能已存在的确认框
        const existingDialog = document.querySelector('.confirmation-dialog-overlay');
        if (existingDialog) {
            existingDialog.remove();
        }
        
        // 创建确认对话框元素
        const overlay = document.createElement('div');
        overlay.className = 'confirmation-dialog-overlay';
        
        const dialog = document.createElement('div');
        dialog.className = 'confirmation-dialog';
        
        dialog.innerHTML = `
            <h4>确认操作</h4>
            <p>${message}</p>
            <div class="confirmation-actions">
                <button class="cancel-button">取消</button>
                <button class="confirm-button">确认</button>
            </div>
        `;
        
        overlay.appendChild(dialog);
        document.body.appendChild(overlay);
        
        // 添加动画效果
        setTimeout(() => {
            overlay.classList.add('show');
            dialog.classList.add('show');
        }, 10);
        
        // 绑定事件
        const cancelButton = dialog.querySelector('.cancel-button');
        const confirmButton = dialog.querySelector('.confirm-button');
        
        cancelButton.addEventListener('click', () => {
            closeDialog();
        });
        
        confirmButton.addEventListener('click', () => {
            closeDialog();
            confirmCallback();
        });
        
        // 点击遮罩层关闭对话框
        overlay.addEventListener('click', (e) => {
            if (e.target === overlay) {
                closeDialog();
            }
        });
        
        // 关闭对话框的函数
        function closeDialog() {
            overlay.classList.remove('show');
            dialog.classList.remove('show');
            
            setTimeout(() => {
                overlay.remove();
            }, 300); // 等待动画完成
        }
    }

    // 修改删除留言的函数，使用自定义确认对话框
    function deleteMessage(messageId) {
        createConfirmDialog('确定要删除这条留言吗？', () => {
            // 确认后执行删除操作
            
            // 先找到DOM中的留言元素
            const messageElement = document.getElementById(`message-${messageId}`);
            if (!messageElement) {
                showNotification('找不到要删除的留言', 'error');
                return;
            }
            
            // 使用与POST接口路径一致的API路径
            fetch(`/api/messages_board/${messageId}`, {
                method: 'DELETE'
            })
            .then(response => {
                if (!response.ok) throw new Error('删除失败');
                
                // 成功删除后:
                // 1. 从内存中的留言数组移除
                messages = messages.filter(msg => msg.id !== messageId);
                
                // 2. 添加淡出动画
                messageElement.style.transition = "all 0.5s ease";
                messageElement.style.opacity = "0";
                messageElement.style.transform = "translateY(-20px)";
                
                // 3. 动画结束后从DOM中移除元素
                setTimeout(() => {
                    messageElement.remove();
                    
                    // 4. 如果列表为空，显示"暂无留言"
                    const messagesContainer = document.getElementById('messagesContainer');
                    if (messagesContainer && messages.length === 0) {
                        messagesContainer.innerHTML = '<div class="no-messages">暂无留言</div>';
                    }
                }, 500);
                
                showNotification('留言已删除', 'success');
            })
            .catch(() => {
                // 即使在后端接口失败的情况下也执行删除操作 (乐观更新)
                // 从内存中的留言数组中移除
                messages = messages.filter(msg => msg.id !== messageId);
                
                // 添加淡出动画
                messageElement.style.transition = "all 0.5s ease";
                messageElement.style.opacity = "0";
                messageElement.style.transform = "translateY(-20px)";
                
                // 动画结束后从DOM中移除元素
                setTimeout(() => {
                    messageElement.remove();
                    
                    // 如果列表为空，显示"暂无留言"
                    const messagesContainer = document.getElementById('messagesContainer');
                    if (messagesContainer && messages.length === 0) {
                        messagesContainer.innerHTML = '<div class="no-messages">暂无留言</div>';
                    }
                }, 500);
                
                showNotification('留言已删除（本地操作）', 'success');
            });
        });
    }

    // 修改删除回复的函数，使用自定义确认对话框
    function deleteReply(messageId, replyId) {
        createConfirmDialog('确定要删除这条回复吗？', () => {
            // 确认后执行删除操作
            
            // 先找到DOM中的回复元素
            const replyElement = document.getElementById(`reply-${replyId}`);
            if (!replyElement) {
                showNotification('找不到要删除的回复', 'error');
                return;
            }
            
            // 使用与留言删除相同的API路径结构
            fetch(`/api/messages_board/${replyId}`, {
                method: 'DELETE'
            })
            .then(response => {
                if (!response.ok) throw new Error('删除失败');
                
                // 成功删除后:
                // 1. 从内存中的回复数组移除
                const message = messages.find(m => m.id === messageId);
                if (message && message.replies) {
                    message.replies = message.replies.filter(r => r.id !== replyId);
                    message.replyCount = message.replies.length;
                    
                    // 更新回复数量显示
                    updateReplyCount(messageId, message.replyCount);
                }
                
                // 2. 添加淡出动画
                replyElement.style.transition = "all 0.5s ease";
                replyElement.style.opacity = "0";
                replyElement.style.transform = "translateY(-20px)";
                
                // 3. 动画结束后从DOM中移除元素
                setTimeout(() => {
                    replyElement.remove();
                    
                    // 4. 如果回复列表为空，显示"暂无回复"
                    const repliesContainer = document.querySelector(`#replies-${messageId} .replies-container`);
                    if (repliesContainer && message.replies.length === 0) {
                        repliesContainer.innerHTML = '<div class="no-replies">暂无回复</div>';
                    }
                }, 500);
                
                showNotification('回复已删除', 'success');
            })
            .catch(() => {
                // 即使在后端接口失败的情况下也执行删除操作 (乐观更新)
                // 从内存中的回复数组中移除
                const message = messages.find(m => m.id === messageId);
                if (message && message.replies) {
                    message.replies = message.replies.filter(r => r.id !== replyId);
                    message.replyCount = message.replies.length;
                    
                    // 更新回复数量显示
                    updateReplyCount(messageId, message.replyCount);
                }
                
                // 添加淡出动画
                replyElement.style.transition = "all 0.5s ease";
                replyElement.style.opacity = "0";
                replyElement.style.transform = "translateY(-20px)";
                
                // 动画结束后从DOM中移除元素
                setTimeout(() => {
                    replyElement.remove();
                    
                    // 4. 如果回复列表为空，显示"暂无回复"
                    const repliesContainer = document.querySelector(`#replies-${messageId} .replies-container`);
                    if (repliesContainer && message.replies.length === 0) {
                        repliesContainer.innerHTML = '<div class="no-replies">暂无回复</div>';
                    }
                }, 500);
                
                showNotification('回复已删除（本地操作）', 'success');
            });
        });
    }

    // 点赞功能
    function toggleLike(messageId) {
        if (!currentUserName || currentUserName === '游客') {
            showLoginNotification('请先登录后才能点赞留言');
            return;
        }

        const message = messages.find(m => m.id === messageId);
        if (!message) {
            console.error(`未找到ID为 ${messageId} 的留言`);
            showNotification('操作失败，请刷新页面后重试', 'error');
            return;
        }
        
        const newLikeStatus = !message.isLiked;
        
        fetch(`/api/messages_board/like/${messageId}`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ liked: newLikeStatus })
        })
            .then(response => {
            if (!response.ok) throw new Error('操作失败');
                return response.json();
            })
            .then(data => {
            message.likes = data.likes;
            message.isLiked = data.isLiked;
            updateLikeUI(messageId, data.likes, data.isLiked);
        })
        .catch(() => {
            // 使用模拟数据
            message.isLiked = newLikeStatus;
            message.likes = (message.likes || 0) + (newLikeStatus ? 1 : -1);
            updateLikeUI(messageId, message.likes, message.isLiked);
        });
    }

    // 更新点赞UI
    function updateLikeUI(messageId, likes, isLiked) {
        const likeBtn = document.querySelector(`#message-${messageId} .action-btn`);
        if (!likeBtn) {
            console.error(`未找到ID为 ${messageId} 的留言点赞按钮`);
            return;
        }
        
        const likeIcon = likeBtn.querySelector('i');
        const likeCount = likeBtn.querySelector('.like-count');
        
        likeBtn.classList.toggle('liked', isLiked);
        likeIcon.className = `fa${isLiked ? 's' : 'r'} fa-heart`;
        likeCount.textContent = likes;
        
        if (isLiked) {
            likeBtn.classList.add('pulse');
            setTimeout(() => likeBtn.classList.remove('pulse'), 500);
        }
    }

    // 更新回复点赞UI
    function updateReplyLikeUI(replyId, likes, isLiked) {
        const replyElement = document.getElementById(`reply-${replyId}`);
        const likeBtn = replyElement.querySelector('.action-btn');
        const likeIcon = likeBtn.querySelector('i');
        const likeCount = likeBtn.querySelector('.like-count');
        
        likeBtn.classList.toggle('liked', isLiked);
        likeIcon.className = `fa${isLiked ? 's' : 'r'} fa-heart`;
        likeCount.textContent = likes;
        
        if (isLiked) {
            likeBtn.classList.add('pulse');
            setTimeout(() => likeBtn.classList.remove('pulse'), 500);
        }
    }

    // 添加退出登录功能
    function logout() {
        // 发送请求到后端进行退出登录操作
        fetch('/api/user/logout', {
            method: 'POST',
            credentials: 'same-origin' // 确保发送cookies
        })
        .then(response => {
            if (!response.ok) {
                throw new Error('退出登录失败');
            }
            return response.json();
        })
        .then(data => {
            showNotification(data.message || '已成功退出登录', 'success');
            // 更新UI为未登录状态
            currentUserName = '游客';
            currentUserAvatar = 'img/default_touxiang.jpg';
            updateUIForAuthStatus(false);
        })
        .catch(error => {
            console.error('退出登录失败:', error);
            showNotification('退出登录请求失败，请重试', 'error');
        });
    }

    // 设置暗黑模式
    function setupDarkMode() {
        // 检查本地存储中的主题偏好
        const isDarkMode = localStorage.getItem('darkMode') === 'true';
        if (isDarkMode) {
            document.body.classList.add('dark-theme');
            updateDarkModeIcon(true);
        }
    }

    // 切换暗黑模式
    function toggleDarkMode() {
        const isDarkMode = document.body.classList.toggle('dark-theme');
        localStorage.setItem('darkMode', isDarkMode);
        updateDarkModeIcon(isDarkMode);
        showNotification(`已切换到${isDarkMode ? '暗色' : '亮色'}模式`, 'success');
    }

    // 更新暗黑模式图标
    function updateDarkModeIcon(isDarkMode) {
        const themeToggle = document.getElementById('themeToggle');
        if (themeToggle) {
            themeToggle.innerHTML = isDarkMode ? 
                '<i class="fas fa-sun"></i>' : 
                '<i class="fas fa-moon"></i>';
        }
    }

    // 新增：判断回复区域是否可见并更新内容
    function updateRepliesIfVisible(messageId, replies) {
        const repliesSection = document.getElementById(`replies-${messageId}`);
        if (repliesSection && repliesSection.style.display !== 'none') {
            // 回复区域可见，更新内容
            const repliesContainer = repliesSection.querySelector('.replies-container');
            if (repliesContainer) {
                repliesContainer.innerHTML = renderRepliesHTML(replies);
            }
        }
    }

    // 处理待处理回复的函数
    function processPendingReplies() {
        if (Object.keys(pendingReplies).length === 0) return; // 没有待处理回复
        
        let updated = false;
        // 检查每个待处理回复
        Object.keys(pendingReplies).forEach(parentId => {
            const existingParentIndex = messages.findIndex(m => m.id == parentId);
            if (existingParentIndex !== -1) {
                // 找到了父留言，更新它的回复列表
                const parent = messages[existingParentIndex];
                if (!parent.replies) parent.replies = [];
                parent.replies = [...parent.replies, ...pendingReplies[parentId]];
                parent.replyCount = parent.replies.length;
                
                // 更新DOM中相应元素的回复计数
                updateReplyCount(parentId, parent.replyCount);
                
                // 如果回复区域已打开，则更新回复内容
                updateRepliesIfVisible(parentId, parent.replies);
                
                // 删除已处理的待处理回复
                delete pendingReplies[parentId];
                updated = true;
            }
        });
        
        if (updated) {
            console.log('已处理一些待处理回复');
        }
    }

    // 显示通知
    function showNotification(message, type = 'info') {
        const notification = document.createElement('div');
        notification.className = `notification ${type}`;
        notification.innerHTML = `
            <i class="fas fa-${type === 'success' ? 'check-circle' : type === 'error' ? 'times-circle' : 'info-circle'}"></i>
            <span>${message}</span>
        `;
        
        document.body.appendChild(notification);
        
        setTimeout(() => {
            notification.remove();
        }, 3000);
    }
});