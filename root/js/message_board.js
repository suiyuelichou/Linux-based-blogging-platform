document.addEventListener('DOMContentLoaded', function() {
    // 全局变量
    let currentPage = 1;
    let totalPages = 1;
    let messagesPerPage = 10;
    let messages = [];
    let currentFilter = 'all';
    let isReplying = null; // 存储当前正在回复的消息ID
    
    // 初始化
    init();
    
    function init() {
        setupEventListeners();
        loadMessages();
        checkAuthStatus();
        setupDarkMode();
    }
    
    // 设置事件监听器
    function setupEventListeners() {
        // 导航菜单切换
        const mobileToggle = document.querySelector('.mobile-toggle');
        const mainNav = document.querySelector('.main-nav');
        
        if (mobileToggle) {
            mobileToggle.addEventListener('click', function() {
                mainNav.classList.toggle('active');
                this.classList.toggle('active');
            });
        }
        
        // 用户头像点击事件
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
        
        // 提交留言
        const submitMessageBtn = document.getElementById('submitMessage');
        if (submitMessageBtn) {
            submitMessageBtn.addEventListener('click', submitMessage);
        }
        
        // 过滤留言
        const filterBtns = document.querySelectorAll('.filter-btn');
        filterBtns.forEach(btn => {
            btn.addEventListener('click', function() {
                const filter = this.getAttribute('data-filter');
                filterMessages(filter);
                
                // 更新活动按钮
                filterBtns.forEach(b => b.classList.remove('active'));
                this.classList.add('active');
            });
        });
        
        // 加载更多
        const loadMoreBtn = document.getElementById('loadMoreBtn');
        if (loadMoreBtn) {
            loadMoreBtn.addEventListener('click', loadMoreMessages);
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
    
    // 加载留言
    function loadMessages() {
        showLoading();
        
        // 发送请求到后端获取留言列表
        fetch(`/api/messages?page=${currentPage}&size=${messagesPerPage}`)
            .then(response => {
                if (!response.ok) {
                    throw new Error('网络响应异常');
                }
                return response.json();
            })
            .then(data => {
                messages = data.messages || [];
                totalPages = data.totalPages || 1;
                
                // 更新统计数据
                if (data.stats) {
                    updateMessageStats(data.stats);
                } else {
                    // 如果没有统计数据，生成默认统计
                    updateMessageStats({
                        totalMessages: messages.length,
                        userCount: new Set(messages.map(m => m.author)).size,
                        likeCount: messages.reduce((sum, msg) => sum + msg.likes, 0)
                    });
                }
                
                renderMessages();
                
                hideLoading();
            })
            .catch(error => {
                console.error('获取留言列表失败:', error);
                // 使用模拟数据作为后备方案
                messages = generateMockMessages(20);
                totalPages = Math.ceil(messages.length / messagesPerPage);
                
                // 当使用模拟数据时，我们需要手动分页
                const startIdx = (currentPage - 1) * messagesPerPage;
                const endIdx = Math.min(startIdx + messagesPerPage, messages.length);
                const pageMessages = messages.slice(startIdx, endIdx);
                
                // 更新统计数据
                updateMessageStats({
                    totalMessages: messages.length,
                    userCount: new Set(messages.map(m => m.author)).size,
                    likeCount: messages.reduce((sum, msg) => sum + msg.likes, 0)
                });
                
                renderMessages(pageMessages);
                
                hideLoading();
                showNotification('获取留言列表失败，显示模拟数据', 'error');
            });
    }
    
    // 更新留言统计数据
    function updateMessageStats(stats) {
        const messageCount = document.getElementById('messageCount');
        const userCount = document.getElementById('userCount');
        const likeCount = document.getElementById('likeCount');
        const totalMessages = document.getElementById('totalMessages');
        
        if (messageCount) messageCount.textContent = stats.totalMessages || 0;
        if (userCount) userCount.textContent = stats.userCount || 0;
        if (likeCount) likeCount.textContent = stats.likeCount || 0;
        if (totalMessages) totalMessages.textContent = stats.totalMessages || 0;
    }
    
    // 加载更多留言
    function loadMoreMessages() {
        const loadMoreBtn = document.getElementById('loadMoreBtn');
        if (loadMoreBtn) {
            loadMoreBtn.classList.add('loading');
            loadMoreBtn.disabled = true;
        }
        
        currentPage++;
        
        // 发送请求到后端获取更多留言
        fetch(`/api/messages?page=${currentPage}&size=${messagesPerPage}&filter=${currentFilter}`)
            .then(response => {
                if (!response.ok) {
                    throw new Error('网络响应异常');
                }
                return response.json();
            })
            .then(data => {
                const newMessages = data.messages || [];
                
                // 将新消息添加到现有消息列表
                if (newMessages.length > 0) {
                    appendMessages(newMessages);
                }
                
                // 检查是否还有更多页
                if (currentPage >= data.totalPages) {
                    const loadMoreBtn = document.getElementById('loadMoreBtn');
                    if (loadMoreBtn) {
                        loadMoreBtn.style.display = 'none';
                    }
                }
                
                if (loadMoreBtn) {
                    loadMoreBtn.classList.remove('loading');
                    loadMoreBtn.disabled = false;
                }
            })
            .catch(error => {
                console.error('加载更多留言失败:', error);
                
                // 使用模拟数据作为后备方案
                const mockMessages = generateMockMessages(10);
                
                if (mockMessages.length > 0) {
                    appendMessages(mockMessages);
                }
                
                // 检查是否还有更多页
                if (currentPage >= totalPages) {
                    const loadMoreBtn = document.getElementById('loadMoreBtn');
                    if (loadMoreBtn) {
                        loadMoreBtn.style.display = 'none';
                    }
                }
                
                if (loadMoreBtn) {
                    loadMoreBtn.classList.remove('loading');
                    loadMoreBtn.disabled = false;
                }
                
                showNotification('加载更多留言失败，显示模拟数据', 'error');
            });
    }
    
    // 过滤留言
    function filterMessages(filter) {
        if (filter === currentFilter) return;
        
        currentFilter = filter;
        currentPage = 1;
        
        // 重新加载留言
        loadMessages();
        
        // 重置加载更多按钮
        const loadMoreBtn = document.getElementById('loadMoreBtn');
        if (loadMoreBtn) {
            loadMoreBtn.style.display = 'block';
        }
    }
    
    // 渲染留言列表
    function renderMessages(customMessages = null) {
        const messagesContainer = document.getElementById('messagesContainer');
        if (!messagesContainer) return;
        
        const messagesToRender = customMessages || messages;
        
        if (currentPage === 1) {
            messagesContainer.innerHTML = '';
        }
        
        if (messagesToRender.length === 0) {
            messagesContainer.innerHTML = '<div class="no-messages">暂无留言</div>';
            return;
        }
        
        // 更新消息索引
        updateMessagesIndex();
        
        let html = '';
        
        messagesToRender.forEach(message => {
            const isOfficial = message.isOfficial;
            const isHighlighted = message.isHighlighted;
            const repliesCount = message.replies ? message.replies.length : 0;
            
            html += `
            <div class="message-item ${isOfficial ? 'official' : ''} ${isHighlighted ? 'highlighted' : ''}" id="message-${message.id}">
                <div class="message-header">
                    <div class="message-author">
                        <img src="${message.authorAvatar}" alt="${message.author}" class="message-author-avatar">
                        <div class="message-author-info">
                            <div class="message-author-name">
                                ${message.author}
                                ${isOfficial ? '<span class="badge official">官方</span>' : ''}
                                ${isHighlighted ? '<span class="badge highlighted">精选</span>' : ''}
                            </div>
                            <div class="message-date">${formatDate(message.date)}</div>
                        </div>
                    </div>
                    <!-- 更多操作按钮，移动端显示 -->
                    <div class="message-more">
                        <button class="message-more-btn"><i class="fas fa-ellipsis-v"></i></button>
                    </div>
                </div>
                <div class="message-content">${formatMessageContent(message.content)}</div>
                <div class="message-actions">
                    <button class="message-action-btn ${message.isLiked ? 'liked' : ''}" onclick="likeMessage(${message.id})">
                        <i class="${message.isLiked ? 'fas' : 'far'} fa-heart"></i> 点赞 <span class="like-count">${message.likes}</span>
                    </button>
                    <button class="message-action-btn" onclick="toggleReplyForm(${message.id})">
                        <i class="far fa-comment"></i> 回复 <span class="reply-count">${repliesCount}</span>
                    </button>
                    <button class="message-action-btn" onclick="shareMessage(${message.id})">
                        <i class="fas fa-share-alt"></i> 分享
                    </button>
                </div>
                
                <!-- 回复区域 -->
                <div class="message-replies" id="replies-${message.id}">
                    ${repliesCount > 0 ? `
                        <button class="reply-toggle" onclick="toggleReplies(${message.id})">
                            <i class="fas fa-chevron-down"></i> 显示全部 ${repliesCount} 条回复
                        </button>
                    ` : ''}
                    <div class="replies-container" id="repliesContainer-${message.id}" style="display: none;">
                        <!-- 回复将通过JS动态加载 -->
                    </div>
                    <div class="reply-form-container" id="replyFormContainer-${message.id}" style="display: none;">
                        <!-- 回复表单将通过JS动态加载 -->
                    </div>
                </div>
            </div>
            `;
        });
        
        // 如果是第一页，则替换内容，否则追加内容
        if (currentPage === 1) {
            messagesContainer.innerHTML = html;
        } else {
            messagesContainer.insertAdjacentHTML('beforeend', html);
        }
        
        // 添加全局函数，用于处理点赞、回复等操作
        window.likeMessage = function(messageId) {
            likeMessage(messageId);
        };
        
        window.toggleReplyForm = function(messageId) {
            toggleReplyForm(messageId);
        };
        
        window.shareMessage = function(messageId) {
            shareMessage(messageId);
        };
        
        window.toggleReplies = function(messageId) {
            toggleReplies(messageId);
        };
        
        window.likeReply = function(messageId, replyId) {
            likeReply(messageId, replyId);
        };
        
        window.submitReply = function(messageId) {
            submitReply(messageId);
        };
        
        window.cancelReply = function(messageId) {
            cancelReply(messageId);
        };
    }
    
    // 追加留言
    function appendMessages(newMessages) {
        const messagesContainer = document.getElementById('messagesContainer');
        if (!messagesContainer) return;
        
        messages = [...messages, ...newMessages];
        
        let html = '';
        
        newMessages.forEach(message => {
            const isOfficial = message.isOfficial;
            const isHighlighted = message.isHighlighted;
            const repliesCount = message.replies ? message.replies.length : 0;
            
            html += `
            <div class="message-item ${isOfficial ? 'official' : ''} ${isHighlighted ? 'highlighted' : ''}" id="message-${message.id}">
                <div class="message-header">
                    <div class="message-author">
                        <img src="${message.authorAvatar}" alt="${message.author}" class="message-author-avatar">
                        <div class="message-author-info">
                            <div class="message-author-name">
                                ${message.author}
                                ${isOfficial ? '<span class="badge official">官方</span>' : ''}
                                ${isHighlighted ? '<span class="badge highlighted">精选</span>' : ''}
                            </div>
                            <div class="message-date">${formatDate(message.date)}</div>
                        </div>
                    </div>
                    <!-- 更多操作按钮，移动端显示 -->
                    <div class="message-more">
                        <button class="message-more-btn"><i class="fas fa-ellipsis-v"></i></button>
                    </div>
                </div>
                <div class="message-content">${formatMessageContent(message.content)}</div>
                <div class="message-actions">
                    <button class="message-action-btn ${message.isLiked ? 'liked' : ''}" onclick="likeMessage(${message.id})">
                        <i class="${message.isLiked ? 'fas' : 'far'} fa-heart"></i> 点赞 <span class="like-count">${message.likes}</span>
                    </button>
                    <button class="message-action-btn" onclick="toggleReplyForm(${message.id})">
                        <i class="far fa-comment"></i> 回复 <span class="reply-count">${repliesCount}</span>
                    </button>
                    <button class="message-action-btn" onclick="shareMessage(${message.id})">
                        <i class="fas fa-share-alt"></i> 分享
                    </button>
                </div>
                
                <!-- 回复区域 -->
                <div class="message-replies" id="replies-${message.id}">
                    ${repliesCount > 0 ? `
                        <button class="reply-toggle" onclick="toggleReplies(${message.id})">
                            <i class="fas fa-chevron-down"></i> 显示全部 ${repliesCount} 条回复
                        </button>
                    ` : ''}
                    <div class="replies-container" id="repliesContainer-${message.id}" style="display: none;">
                        <!-- 回复将通过JS动态加载 -->
                    </div>
                    <div class="reply-form-container" id="replyFormContainer-${message.id}" style="display: none;">
                        <!-- 回复表单将通过JS动态加载 -->
                    </div>
                </div>
            </div>
            `;
        });
        
        messagesContainer.insertAdjacentHTML('beforeend', html);
    }
    
    // 切换回复列表显示/隐藏
    function toggleReplies(messageId) {
        const repliesContainer = document.getElementById(`repliesContainer-${messageId}`);
        const replyToggle = document.querySelector(`#replies-${messageId} .reply-toggle`);
        
        if (!repliesContainer || !replyToggle) return;
        
        const isVisible = repliesContainer.style.display !== 'none';
        
        if (isVisible) {
            repliesContainer.style.display = 'none';
            replyToggle.innerHTML = `<i class="fas fa-chevron-down"></i> 显示全部 ${messagesById[messageId].replies.length} 条回复`;
        } else {
            repliesContainer.style.display = 'block';
            replyToggle.innerHTML = `<i class="fas fa-chevron-up"></i> 收起回复`;
            
            // 加载回复
            loadReplies(messageId);
        }
    }
    
    // 加载回复
    function loadReplies(messageId) {
        const repliesContainer = document.getElementById(`repliesContainer-${messageId}`);
        if (!repliesContainer) return;
        
        // 找到当前消息
        const message = messages.find(msg => msg.id === messageId);
        if (!message || !message.replies) return;
        
        let html = '';
        
        message.replies.forEach(reply => {
            html += `
            <div class="reply-item" id="reply-${messageId}-${reply.id}">
                <img src="${reply.authorAvatar}" alt="${reply.author}" class="reply-avatar">
                <div class="reply-content">
                    <div class="reply-header">
                        <div class="reply-author">
                            ${reply.author}
                            ${reply.isOfficial ? '<span class="badge official">官方</span>' : ''}
                        </div>
                        <div class="reply-date">${formatDate(reply.date)}</div>
                    </div>
                    <div class="reply-text">${formatMessageContent(reply.content)}</div>
                    <div class="reply-actions">
                        <button class="reply-action-btn ${reply.isLiked ? 'liked' : ''}" onclick="likeReply(${messageId}, ${reply.id})">
                            <i class="${reply.isLiked ? 'fas' : 'far'} fa-heart"></i> <span class="reply-like-count">${reply.likes}</span>
                        </button>
                    </div>
                </div>
            </div>
            `;
        });
        
        repliesContainer.innerHTML = html;
    }
    
    // 切换回复表单显示/隐藏
    function toggleReplyForm(messageId) {
        const replyFormContainer = document.getElementById(`replyFormContainer-${messageId}`);
        if (!replyFormContainer) return;
        
        const isVisible = replyFormContainer.style.display !== 'none';
        
        // 如果已经在显示中，则隐藏
        if (isVisible) {
            replyFormContainer.style.display = 'none';
            isReplying = null; // 重置当前回复状态
            return;
        }
        
        // 确保用户已登录
        checkAuthStatusForAction(() => {
            // 更新当前回复状态
            isReplying = messageId;
            
            // 生成回复表单
            const formHtml = `
            <div class="reply-form" id="replyForm-${messageId}">
                <img src="${currentUserAvatar || 'img/default_touxiang.jpg'}" alt="头像" class="reply-form-avatar">
                <div class="reply-form-input">
                    <textarea placeholder="写下你的回复..."></textarea>
                    <div class="reply-form-actions">
                        <div>
                            <button type="button" class="reply-cancel-btn" onclick="cancelReply(${messageId})">取消</button>
                            <button type="button" class="reply-submit-btn" onclick="submitReply(${messageId})">发表回复</button>
                        </div>
                    </div>
                </div>
            </div>
            `;
            
            replyFormContainer.innerHTML = formHtml;
            replyFormContainer.style.display = 'block';
            
            // 聚焦到输入框
            const textarea = replyFormContainer.querySelector('textarea');
            if (textarea) {
                textarea.focus();
            }
        });
    }
    
    // 取消回复
    window.cancelReply = function(messageId) {
        const replyFormContainer = document.getElementById(`replyFormContainer-${messageId}`);
        if (replyFormContainer) {
            replyFormContainer.style.display = 'none';
            replyFormContainer.innerHTML = '';
        }
        
        isReplying = null; // 重置当前回复状态
    };
    
    // 提交回复
    window.submitReply = function(messageId) {
        const replyForm = document.getElementById(`replyForm-${messageId}`);
        if (!replyForm) return;
        
        const textarea = replyForm.querySelector('textarea');
        if (!textarea || !textarea.value.trim()) {
            showNotification('回复内容不能为空', 'error');
            return;
        }
        
        const replyContent = textarea.value.trim();
        
        // 添加回复提交中的加载状态
        const submitBtn = replyForm.querySelector('.reply-submit-btn');
        if (submitBtn) {
            submitBtn.disabled = true;
            submitBtn.innerHTML = '<i class="fas fa-spinner fa-spin"></i> 提交中...';
        }
        
        // 发送请求到后端提交回复
        fetch(`/api/messages/${messageId}/replies`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                content: replyContent
            })
        })
        .then(response => {
            if (!response.ok) {
                throw new Error('提交回复失败');
            }
            return response.json();
        })
        .then(data => {
            // 更新回复列表
            const message = messages.find(msg => msg.id === messageId);
            if (message) {
                // 如果replies还不存在，初始化为空数组
                if (!message.replies) {
                    message.replies = [];
                }
                
                // 添加新回复
                message.replies.push(data.reply);
                
                // 更新回复计数
                const replyCountElement = document.querySelector(`#message-${messageId} .reply-count`);
                if (replyCountElement) {
                    replyCountElement.textContent = message.replies.length;
                }
                
                // 更新回复切换按钮
                const replyToggle = document.querySelector(`#replies-${messageId} .reply-toggle`);
                if (replyToggle) {
                    if (message.replies.length === 1) {
                        // 如果这是第一个回复，添加回复切换按钮
                        const repliesDiv = document.getElementById(`replies-${messageId}`);
                        if (repliesDiv && !replyToggle) {
                            const toggleHtml = `
                            <button class="reply-toggle" onclick="toggleReplies(${messageId})">
                                <i class="fas fa-chevron-down"></i> 显示全部 1 条回复
                            </button>
                            `;
                            repliesDiv.insertAdjacentHTML('afterbegin', toggleHtml);
                        }
                    } else {
                        // 更新现有的回复切换按钮
                        replyToggle.innerHTML = `<i class="fas fa-chevron-down"></i> 显示全部 ${message.replies.length} 条回复`;
                    }
                }
                
                // 加载回复并打开回复列表
                const repliesContainer = document.getElementById(`repliesContainer-${messageId}`);
                if (repliesContainer) {
                    repliesContainer.style.display = 'block';
                    loadReplies(messageId);
                }
                
                // 隐藏回复表单
                cancelReply(messageId);
                
                // 显示成功通知
                showNotification('回复发表成功', 'success');
            }
        })
        .catch(error => {
            console.error('提交回复失败:', error);
            
            // 使用模拟数据作为后备方案
            const message = messages.find(msg => msg.id === messageId);
            if (message) {
                // 如果replies还不存在，初始化为空数组
                if (!message.replies) {
                    message.replies = [];
                }
                
                // 创建模拟回复
                const mockReply = {
                    id: Date.now(),
                    author: currentUserName || '用户',
                    authorAvatar: currentUserAvatar || 'img/default_touxiang.jpg',
                    content: replyContent,
                    date: new Date().toISOString(),
                    likes: 0,
                    isLiked: false,
                    isOfficial: false
                };
                
                // 添加新回复
                message.replies.push(mockReply);
                
                // 更新回复计数
                const replyCountElement = document.querySelector(`#message-${messageId} .reply-count`);
                if (replyCountElement) {
                    replyCountElement.textContent = message.replies.length;
                }
                
                // 更新回复切换按钮
                const replyToggle = document.querySelector(`#replies-${messageId} .reply-toggle`);
                if (replyToggle) {
                    if (message.replies.length === 1) {
                        // 如果这是第一个回复，添加回复切换按钮
                        const repliesDiv = document.getElementById(`replies-${messageId}`);
                        if (repliesDiv && !replyToggle) {
                            const toggleHtml = `
                            <button class="reply-toggle" onclick="toggleReplies(${messageId})">
                                <i class="fas fa-chevron-down"></i> 显示全部 1 条回复
                            </button>
                            `;
                            repliesDiv.insertAdjacentHTML('afterbegin', toggleHtml);
                        }
                    } else {
                        // 更新现有的回复切换按钮
                        replyToggle.innerHTML = `<i class="fas fa-chevron-down"></i> 显示全部 ${message.replies.length} 条回复`;
                    }
                }
                
                // 加载回复并打开回复列表
                const repliesContainer = document.getElementById(`repliesContainer-${messageId}`);
                if (repliesContainer) {
                    repliesContainer.style.display = 'block';
                    loadReplies(messageId);
                }
                
                // 隐藏回复表单
                cancelReply(messageId);
                
                // 显示成功通知
                showNotification('回复发表成功（模拟数据）', 'success');
            }
        })
        .finally(() => {
            // 恢复按钮状态
            if (submitBtn) {
                submitBtn.disabled = false;
                submitBtn.innerHTML = '发表回复';
            }
        });
    };
    
    // 点赞留言
    function likeMessage(messageId) {
        // 确保用户已登录
        checkAuthStatusForAction(() => {
            const messageEl = document.getElementById(`message-${messageId}`);
            if (!messageEl) return;
            
            const likeBtn = messageEl.querySelector('.message-action-btn');
            const likeIcon = likeBtn.querySelector('i');
            const likeCount = likeBtn.querySelector('.like-count');
            
            // 查找该消息
            const message = messages.find(msg => msg.id === messageId);
            if (!message) return;
            
            // 切换点赞状态
            message.isLiked = !message.isLiked;
            message.likes += message.isLiked ? 1 : -1;
            
            // 更新UI
            if (message.isLiked) {
                likeBtn.classList.add('liked');
                likeIcon.className = 'fas fa-heart';
                likeBtn.classList.add('pulse'); // 添加动画效果
                setTimeout(() => {
                    likeBtn.classList.remove('pulse');
                }, 500);
            } else {
                likeBtn.classList.remove('liked');
                likeIcon.className = 'far fa-heart';
            }
            
            likeCount.textContent = message.likes;
            
            // 发送请求到后端更新点赞状态
            fetch(`/api/messages/${messageId}/like`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({
                    liked: message.isLiked
                })
            })
            .catch(error => {
                console.error('更新点赞状态失败:', error);
                
                // 如果请求失败，回滚状态
                message.isLiked = !message.isLiked;
                message.likes += message.isLiked ? 1 : -1;
                
                if (message.isLiked) {
                    likeBtn.classList.add('liked');
                    likeIcon.className = 'fas fa-heart';
                } else {
                    likeBtn.classList.remove('liked');
                    likeIcon.className = 'far fa-heart';
                }
                
                likeCount.textContent = message.likes;
                
                showNotification('点赞操作失败，请重试', 'error');
            });
        });
    }
    
    // 点赞回复
    window.likeReply = function(messageId, replyId) {
        // 确保用户已登录
        checkAuthStatusForAction(() => {
            const replyEl = document.getElementById(`reply-${messageId}-${replyId}`);
            if (!replyEl) return;
            
            const likeBtn = replyEl.querySelector('.reply-action-btn');
            const likeIcon = likeBtn.querySelector('i');
            const likeCount = likeBtn.querySelector('.reply-like-count');
            
            // 查找该消息和回复
            const message = messages.find(msg => msg.id === messageId);
            if (!message || !message.replies) return;
            
            const reply = message.replies.find(rpl => rpl.id === replyId);
            if (!reply) return;
            
            // 切换点赞状态
            reply.isLiked = !reply.isLiked;
            reply.likes += reply.isLiked ? 1 : -1;
            
            // 更新UI
            if (reply.isLiked) {
                likeBtn.classList.add('liked');
                likeIcon.className = 'fas fa-heart';
                likeBtn.classList.add('pulse'); // 添加动画效果
                setTimeout(() => {
                    likeBtn.classList.remove('pulse');
                }, 500);
            } else {
                likeBtn.classList.remove('liked');
                likeIcon.className = 'far fa-heart';
            }
            
            likeCount.textContent = reply.likes;
            
            // 发送请求到后端更新点赞状态
            fetch(`/api/messages/${messageId}/replies/${replyId}/like`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({
                    liked: reply.isLiked
                })
            })
            .catch(error => {
                console.error('更新回复点赞状态失败:', error);
                
                // 如果请求失败，回滚状态
                reply.isLiked = !reply.isLiked;
                reply.likes += reply.isLiked ? 1 : -1;
                
                if (reply.isLiked) {
                    likeBtn.classList.add('liked');
                    likeIcon.className = 'fas fa-heart';
                } else {
                    likeBtn.classList.remove('liked');
                    likeIcon.className = 'far fa-heart';
                }
                
                likeCount.textContent = reply.likes;
                
                showNotification('点赞操作失败，请重试', 'error');
            });
        });
    };
    
    // 分享留言
    function shareMessage(messageId) {
        // 查找该消息
        const message = messages.find(msg => msg.id === messageId);
        if (!message) return;
        
        // 生成分享链接
        const shareUrl = `${window.location.origin}/message_board.html?message=${messageId}`;
        
        // 检查是否支持navigator.share
        if (navigator.share) {
            navigator.share({
                title: `来自 ${message.author} 的留言 - 云中杉木博客`,
                text: message.content.length > 100 ? message.content.substring(0, 100) + '...' : message.content,
                url: shareUrl
            })
            .then(() => console.log('分享成功'))
            .catch(error => console.error('分享失败:', error));
        } else {
            // 如果不支持navigator.share，则复制链接到剪贴板
            navigator.clipboard.writeText(shareUrl)
                .then(() => {
                    showNotification('链接已复制到剪贴板', 'success');
                })
                .catch(err => {
                    console.error('复制失败:', err);
                    showNotification('复制链接失败，请手动复制: ' + shareUrl, 'error');
                });
        }
    }
    
    // 格式化留言内容
    function formatMessageContent(content) {
        if (!content) return '';
        
        // 将URL转换为可点击的链接
        const urlRegex = /(https?:\/\/[^\s]+)/g;
        
        // 替换URL，保留换行符
        let formattedContent = content
            .replace(urlRegex, url => `<a href="${url}" target="_blank">${url}</a>`)
            .replace(/\n/g, '<br>');
        
        return formattedContent;
    }
    
    // 格式化日期
    function formatDate(dateString) {
        if (!dateString) return '';
        
        const now = new Date();
        const date = new Date(dateString);
        const diffTime = Math.abs(now - date);
        const diffDays = Math.floor(diffTime / (1000 * 60 * 60 * 24));
        
        if (diffDays === 0) {
            // 今天，显示时间
            const hours = date.getHours().toString().padStart(2, '0');
            const minutes = date.getMinutes().toString().padStart(2, '0');
            return `今天 ${hours}:${minutes}`;
        } else if (diffDays === 1) {
            // 昨天
            return '昨天';
        } else if (diffDays < 7) {
            // 一周内
            return `${diffDays}天前`;
        } else {
            // 超过一周，显示完整日期
            const year = date.getFullYear();
            const month = (date.getMonth() + 1).toString().padStart(2, '0');
            const day = date.getDate().toString().padStart(2, '0');
            return `${year}-${month}-${day}`;
        }
    }
    
    // 检查登录状态
    function checkAuthStatus() {
        // 发送请求到后端检查用户登录状态
        fetch('/api/user/info')
            .then(response => {
                if (!response.ok) {
                    if (response.status === 401) {
                        // 用户未登录，使用游客状态
                        updateUIForAuthStatus(false);
                        return null;
                    }
                    throw new Error('网络响应异常');
                }
                return response.json();
            })
            .then(data => {
                if (data) {
                    // 用户已登录，更新UI显示用户信息
                    updateUIForAuthStatus(true, data);
                    
                    // 更新全局用户信息
                    currentUserName = data.username || '用户';
                    currentUserAvatar = data.avatar || 'img/default_touxiang.jpg';
                }
            })
            .catch(error => {
                console.error('检查登录状态失败:', error);
                // 默认使用未登录状态
                updateUIForAuthStatus(false);
            });
    }
    
    // 全局变量，用于存储当前用户信息
    let currentUserName = null;
    let currentUserAvatar = null;
    
    // 更新为接收用户数据的版本
    function updateUIForAuthStatus(isLoggedIn, userData) {
        const userAvatar = document.getElementById('userAvatar');
        const currentUserAvatar = document.getElementById('currentUserAvatar');
        const sidebarUserAvatar = document.getElementById('sidebarUserAvatar');
        const sidebarUserName = document.getElementById('sidebarUserName');
        const userDropdown = document.querySelector('.user-dropdown');
        const userMessages = document.getElementById('userMessages');
        const userReplies = document.getElementById('userReplies');
        const userLikes = document.getElementById('userLikes');
        
        if (isLoggedIn && userData) {
            // 更新全局变量
            currentUserName = userData.username || '用户';
            currentUserAvatar = userData.avatar || 'img/default_touxiang.jpg';
            
            // 用户已登录，显示用户信息
            if (userAvatar) userAvatar.src = userData.avatar || 'img/default_touxiang.jpg';
            if (currentUserAvatar) currentUserAvatar.src = userData.avatar || 'img/default_touxiang.jpg';
            if (sidebarUserAvatar) sidebarUserAvatar.src = userData.avatar || 'img/default_touxiang.jpg';
            if (sidebarUserName) sidebarUserName.textContent = userData.username || '用户';
            if (userMessages) userMessages.textContent = userData.messageCount || '0';
            if (userReplies) userReplies.textContent = userData.replyCount || '0';
            if (userLikes) userLikes.textContent = userData.likeCount || '0';
            
            // 更新留言板统计数据
            updateMessageStats({
                totalMessages: userData.messageCount || 0,
                userCount: userData.userCount || 0,
                likeCount: userData.likeCount || 0
            });
            
            // 更新下拉菜单
            if (userDropdown) {
                userDropdown.innerHTML = `
                <a href="user_center.html"><i class="fas fa-user-circle"></i> 个人中心</a>
                <a href="blog_editor.html"><i class="fas fa-edit"></i> 写博客</a>
                <a href="blog_settings.html"><i class="fas fa-cog"></i> 设置</a>
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
            // 更新全局变量
            currentUserName = null;
            currentUserAvatar = null;
            
            // 用户未登录，显示游客信息
            if (userAvatar) userAvatar.src = 'img/default_touxiang.jpg';
            if (currentUserAvatar) currentUserAvatar.src = 'img/default_touxiang.jpg';
            if (sidebarUserAvatar) sidebarUserAvatar.src = 'img/default_touxiang.jpg';
            if (sidebarUserName) sidebarUserName.textContent = '游客';
            if (userMessages) userMessages.textContent = '0';
            if (userReplies) userReplies.textContent = '0';
            if (userLikes) userLikes.textContent = '0';
            
            // 更新下拉菜单
            if (userDropdown) {
                userDropdown.innerHTML = `
                <a href="blog_login.html"><i class="fas fa-sign-in-alt"></i> 登录</a>
                <a href="blog_register.html"><i class="fas fa-user-plus"></i> 注册</a>
                `;
            }
        }
    }
    
    // 检查用户登录状态并执行操作
    function checkAuthStatusForAction(callback) {
        if (currentUserName) {
            // 用户已登录，直接执行回调
            callback();
        } else {
            // 用户未登录，显示登录提示
            showLoginNotification('请先登录后再进行操作', 'info');
        }
    }
    
    // 显示带有登录按钮的通知
    function showLoginNotification(message, type = 'info') {
        // 先删除可能存在的通知
        const existingNotification = document.querySelector('.notification');
        if (existingNotification) {
            existingNotification.remove();
        }
        
        // 创建新通知
        const notification = document.createElement('div');
        notification.className = `notification with-button ${type}`;
        
        let icon = '';
        switch (type) {
            case 'success':
                icon = '<i class="fas fa-check-circle"></i>';
                break;
            case 'error':
                icon = '<i class="fas fa-exclamation-circle"></i>';
                break;
            default:
                icon = '<i class="fas fa-info-circle"></i>';
        }
        
        notification.innerHTML = `
            ${icon}
            <div class="notification-message">${message}</div>
            <a href="blog_login.html" class="notification-login-btn">立即登录</a>
            <div class="notification-close"><i class="fas fa-times"></i></div>
        `;
        
        document.body.appendChild(notification);
        
        // 显示通知
        setTimeout(() => {
            notification.classList.add('show');
        }, 10);
        
        // 添加关闭事件
        const closeBtn = notification.querySelector('.notification-close');
        closeBtn.addEventListener('click', () => {
            notification.classList.remove('show');
            setTimeout(() => {
                notification.remove();
            }, 300);
        });
        
        // 自动关闭
        setTimeout(() => {
            notification.classList.remove('show');
            setTimeout(() => {
                notification.remove();
            }, 300);
        }, 8000);
    }
    
    // 退出登录
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
    
    // 显示加载中状态
    function showLoading() {
        const messagesContainer = document.getElementById('messagesContainer');
        if (!messagesContainer) return;
        
        let skeletonHTML = '';
        for (let i = 0; i < 5; i++) {
            skeletonHTML += `
            <div class="message-item skeleton-item">
                <div class="skeleton-header">
                    <div class="skeleton-avatar skeleton"></div>
                    <div class="skeleton-info">
                        <div class="skeleton-name skeleton"></div>
                        <div class="skeleton-date skeleton"></div>
                    </div>
                </div>
                <div class="skeleton-content skeleton"></div>
                <div class="skeleton-content skeleton" style="width: 85%;"></div>
                <div class="skeleton-actions">
                    <div class="skeleton-action skeleton"></div>
                    <div class="skeleton-action skeleton"></div>
                    <div class="skeleton-action skeleton"></div>
                </div>
            </div>
            `;
        }
        
        messagesContainer.innerHTML = skeletonHTML;
    }
    
    // 隐藏加载中状态
    function hideLoading() {
        // 加载完成后，已经渲染了实际内容，所以不需要特别处理
    }
    
    // 显示通知消息
    function showNotification(message, type = 'info') {
        // 先删除可能存在的通知
        const existingNotification = document.querySelector('.notification:not(.with-button)');
        if (existingNotification) {
            existingNotification.remove();
        }
        
        // 创建新通知
        const notification = document.createElement('div');
        notification.className = `notification ${type}`;
        
        let icon = '';
        switch (type) {
            case 'success':
                icon = '<i class="fas fa-check-circle"></i>';
                break;
            case 'error':
                icon = '<i class="fas fa-exclamation-circle"></i>';
                break;
            default:
                icon = '<i class="fas fa-info-circle"></i>';
        }
        
        notification.innerHTML = `
            ${icon}
            <div class="notification-message">${message}</div>
            <div class="notification-close"><i class="fas fa-times"></i></div>
        `;
        
        document.body.appendChild(notification);
        
        // 显示通知
        setTimeout(() => {
            notification.classList.add('show');
        }, 10);
        
        // 添加关闭事件
        const closeBtn = notification.querySelector('.notification-close');
        closeBtn.addEventListener('click', () => {
            notification.classList.remove('show');
            setTimeout(() => {
                notification.remove();
            }, 300);
        });
        
        // 自动关闭
        setTimeout(() => {
            notification.classList.remove('show');
            setTimeout(() => {
                notification.remove();
            }, 300);
        }, 5000);
    }
    
    // 生成模拟留言数据
    function generateMockMessages(count) {
        const mockMessages = [];
        const avatars = [
            'img/avatar1.jpg',
            'img/avatar2.jpg',
            'img/avatar3.jpg',
            'img/default_touxiang.jpg'
        ];
        const usernames = [
            '云中杉木',
            '技术小能手',
            '编程达人',
            '前端小王子',
            '代码女神',
            'Linux爱好者',
            '全栈工程师'
        ];
        const contents = [
            '博客平台做得很棒，我很喜欢这个设计！',
            '我是一名前端开发者，希望能和博主交流学习。',
            '请问有关于Vue.js的教程吗？我想深入学习一下。',
            '博主的文章写得非常好，我学到了很多知识，谢谢分享！',
            '网站UI设计很现代，用户体验也很好，点赞！',
            '希望能看到更多关于后端开发的内容，特别是Node.js方面的。',
            '我是刚入行的新手，博主的博客对我帮助很大，感谢！',
            '这个博客平台是自己开发的吗？技术栈是什么？能分享一下吗？',
            '博主更新太慢了吧，都一个星期没有新文章了 😂',
            '请问如何优化网站性能？我的网站加载速度很慢。',
            '我在学习React，有没有推荐的学习资源？',
            '博主的代码示例非常清晰，让我很容易理解复杂的概念！',
            '期待更多关于人工智能和机器学习的内容。'
        ];
        
        for (let i = 1; i <= count; i++) {
            const username = usernames[Math.floor(Math.random() * usernames.length)];
            const avatar = avatars[Math.floor(Math.random() * avatars.length)];
            const content = contents[Math.floor(Math.random() * contents.length)];
            const isOfficial = username === '云中杉木';
            const isHighlighted = Math.random() > 0.8;
            const likes = Math.floor(Math.random() * 50);
            const now = new Date();
            const randomDays = Math.floor(Math.random() * 30);
            const date = new Date(now.getTime() - randomDays * 24 * 60 * 60 * 1000);
            
            // 生成随机回复
            const replies = [];
            const replyCount = Math.floor(Math.random() * 5);
            
            for (let j = 1; j <= replyCount; j++) {
                const replyUsername = usernames[Math.floor(Math.random() * usernames.length)];
                const replyAvatar = avatars[Math.floor(Math.random() * avatars.length)];
                const replyContent = '回复 @' + username + '：' + contents[Math.floor(Math.random() * contents.length)];
                const replyIsOfficial = replyUsername === '云中杉木';
                const replyLikes = Math.floor(Math.random() * 20);
                const replyDate = new Date(date.getTime() + Math.floor(Math.random() * 2) * 24 * 60 * 60 * 1000);
                
                replies.push({
                    id: j,
                    author: replyUsername,
                    authorAvatar: replyAvatar,
                    content: replyContent,
                    date: replyDate.toISOString(),
                    likes: replyLikes,
                    isLiked: Math.random() > 0.8,
                    isOfficial: replyIsOfficial
                });
            }
            
            mockMessages.push({
                id: i,
                author: username,
                authorAvatar: avatar,
                content: content,
                date: date.toISOString(),
                likes: likes,
                isLiked: Math.random() > 0.8,
                isOfficial: isOfficial,
                isHighlighted: isHighlighted,
                replies: replies
            });
        }
        
        return mockMessages;
    }

    // 提交留言函数
    function submitMessage() {
        const messageInput = document.getElementById('messageInput');
        if (!messageInput || !messageInput.value.trim()) {
            showNotification('留言内容不能为空', 'error');
            return;
        }
        
        // 确保用户已登录
        checkAuthStatusForAction(() => {
            const content = messageInput.value.trim();
            
            // 显示提交中状态
            const submitBtn = document.getElementById('submitMessage');
            if (submitBtn) {
                submitBtn.disabled = true;
                submitBtn.innerHTML = '<i class="fas fa-spinner fa-spin"></i> 提交中...';
            }
            
            // 发送请求到后端
            fetch('/api/messages', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({
                    content: content
                })
            })
            .then(response => {
                if (!response.ok) {
                    throw new Error('提交留言失败');
                }
                return response.json();
            })
            .then(data => {
                // 成功提交
                messageInput.value = '';
                showNotification('留言发表成功', 'success');
                
                // 将新留言添加到列表顶部
                if (data.message) {
                    messages.unshift(data.message);
                    renderMessages();
                }
            })
            .catch(error => {
                console.error('提交留言失败:', error);
                
                // 使用模拟数据作为后备方案
                const mockMessage = {
                    id: Date.now(),
                    author: currentUserName || '用户',
                    authorAvatar: currentUserAvatar || 'img/default_touxiang.jpg',
                    content: content,
                    date: new Date().toISOString(),
                    likes: 0,
                    isLiked: false,
                    isOfficial: false,
                    isHighlighted: false,
                    replies: []
                };
                
                // 将新留言添加到列表顶部
                messages.unshift(mockMessage);
                renderMessages();
                
                // 清空输入框
                messageInput.value = '';
                
                showNotification('留言发表成功（模拟数据）', 'success');
            })
            .finally(() => {
                // 恢复按钮状态
                if (submitBtn) {
                    submitBtn.disabled = false;
                    submitBtn.innerHTML = '发布留言';
                }
            });
        });
    }

    // 创建一个消息索引对象，用于快速通过ID访问消息
    function updateMessagesIndex() {
        window.messagesById = {};
        messages.forEach(message => {
            messagesById[message.id] = message;
        });
    }
});