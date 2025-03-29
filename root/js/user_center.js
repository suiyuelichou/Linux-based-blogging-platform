document.addEventListener('DOMContentLoaded', function() {
    // 初始化页面
    initUserCenter();
    
    // 绑定事件
    bindEvents();
    
    // 设置主题切换
    setupThemeToggle();
    
    // 加载默认页面内容
    loadPageData('profile');
});

// 初始化用户中心
function initUserCenter() {
    // 获取用户信息
    fetchUserInfo();
    
    // 检查主题设置
    checkThemeSetting();
    
    // 初始化移动菜单
    initMobileMenu();
}

// 获取用户信息 - 修改版
function fetchUserInfo() {
    // 显示加载状态
    document.getElementById('userName').textContent = '加载中...';
    
    // 发送请求到后端API获取用户信息
    fetch('/api/user/info')
        .then(response => {
            if (!response.ok) {
                throw new Error('获取用户信息失败');
            }
            return response.json();
        })
        .then(userData => {
            // 使用获取到的用户数据更新UI
            updateUIForAuthStatus(true, userData);
        })
        .catch(error => {
            console.error('获取用户信息失败:', error);
            // 显示游客状态
            updateUIForAuthStatus(false);
            
            // 错误通知
            showNotification('获取用户信息失败，请重新登录', 'error');
        });
}

// 退出登录功能
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
        
        // 延迟跳转到登录页
        setTimeout(() => {
            window.location.href = 'blog_login.html';
        }, 1500);
    })
    .catch(error => {
        console.error('退出登录失败:', error);
        showNotification('退出登录请求失败，请重试', 'error');
    });
}

// 用户信息UI更新函数，适配新的响应格式
function updateUIForAuthStatus(isLoggedIn, userData = null) {
    const userAvatar = document.getElementById('userAvatar');
    const userDropdown = document.querySelector('.user-dropdown');
    const avatarImage = document.getElementById('avatarImage');
    const userName = document.getElementById('userName');
    
    if (isLoggedIn && userData) {
        // 用户已登录，显示用户信息
        if (userAvatar) userAvatar.src = userData.avatar || 'img/default_touxiang.jpg';
        if (avatarImage) avatarImage.src = userData.avatar || 'img/default_touxiang.jpg';
        if (userName) userName.textContent = userData.username || '用户';
        
        // 更新下拉菜单
        if (userDropdown) {
            userDropdown.innerHTML = `
                <a href="user_center.html" class="active"><i class="fas fa-user-circle"></i> 个人中心</a>
                <a href="blog_editor.html"><i class="fas fa-edit"></i> 写博客</a>
                <a href="blog_settings.html"><i class="fas fa-cog"></i> 设置</a>
                <a href="#" id="logout"><i class="fas fa-sign-out-alt"></i> 退出登录</a>
            `;
            
            // 重新绑定退出登录事件
            const logoutBtn = userDropdown.querySelector('#logout');
            if (logoutBtn) {
                logoutBtn.addEventListener('click', function(e) {
                    e.preventDefault();
                    if (confirm('确定要退出登录吗？')) {
                        logout();
                    }
                });
            }
        }
        
        // 如果个人资料页面已加载，更新其中的信息
        updateProfileContent(userData);
    } else {
        // 用户未登录，显示默认信息
        if (userAvatar) userAvatar.src = 'img/default_touxiang.jpg';
        if (avatarImage) avatarImage.src = 'img/default_touxiang.jpg';
        if (userName) userName.textContent = '游客';
        
        // 更新下拉菜单
        if (userDropdown) {
            userDropdown.innerHTML = `
                <a href="blog_login.html"><i class="fas fa-sign-in-alt"></i> 登录</a>
                <a href="blog_register.html"><i class="fas fa-user-plus"></i> 注册</a>
            `;
        }
    }
}

// 更新个人资料页面内容 - 直接通过生成新的HTML来更新内容
function updateProfileContent(userData) {
    const contentArea = document.getElementById('userContent');
    if (!contentArea || !userData || contentArea.querySelector('h2')?.textContent !== '个人资料') {
        return; // 不是个人资料页面或没有数据
    }
    
    const bioText = userData.bio ? userData.bio : '这个人很懒，什么都没写...';
    
    // 基本信息部分
    const basicInfoSection = contentArea.querySelector('.profile-section:nth-child(1)');
    if (basicInfoSection) {
        basicInfoSection.innerHTML = `
            <h3>基本信息</h3>
            <div class="info-item">
                <label>用户名</label>
                <span>${userData.username || '未设置'}</span>
            </div>
            <div class="info-item">
                <label>邮箱</label>
                <span>${userData.email || '未设置'}</span>
            </div>
            <div class="info-item">
                <label>注册时间</label>
                <span>${userData.registerDate || '未知'}</span>
            </div>
            <div class="info-item">
                <label>个人简介</label>
                <p>${bioText}</p>
            </div>
        `;
    }
    
    // 统计数据部分
    const statsSection = contentArea.querySelector('.profile-stats');
    if (statsSection) {
        statsSection.innerHTML = `
            <div class="stat-item">
                <div class="stat-value">${userData.articleCount || '0'}</div>
                <div class="stat-label">发布博客</div>
            </div>
            <div class="stat-item">
                <div class="stat-value">${userData.viewCount || '0'}</div>
                <div class="stat-label">总浏览量</div>
            </div>
            <div class="stat-item">
                <div class="stat-value">${userData.likeCount || '0'}</div>
                <div class="stat-label">获赞数</div>
            </div>
            <div class="stat-item">
                <div class="stat-value">${userData.commentCount || '0'}</div>
                <div class="stat-label">评论数</div>
            </div>
        `;
    }
}

// 绑定事件
function bindEvents() {
    // 侧边栏菜单点击事件
    const navLinks = document.querySelectorAll('.user-nav-links li');
    navLinks.forEach(link => {
        link.addEventListener('click', function() {
            // 移除所有选中
            navLinks.forEach(item => item.classList.remove('active'));
            // 设置当前选中
            this.classList.add('active');
            // 加载对应页面内容
            loadPageData(this.getAttribute('data-page'));
        });
    });
    
    // 头像上传
    const avatarUpload = document.getElementById('avatar-upload');
    const avatarImage = document.getElementById('avatarImage');
    
    avatarUpload.addEventListener('change', function(e) {
        const file = e.target.files[0];
        if (file) {
            const reader = new FileReader();
            reader.onload = function(e) {
                avatarImage.src = e.target.result;
                document.getElementById('userAvatar').src = e.target.result;
                
                // 这里应该增加上传头像到服务器的代码
                uploadAvatarToServer(file);
            };
            reader.readAsDataURL(file);
        }
    });
    
    avatarImage.addEventListener('click', function() {
        document.getElementById('avatar-upload').click();
    });
    
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

        // 阻止下拉菜单内的点击事件冒泡
        userDropdown.addEventListener('click', function(e) {
            e.stopPropagation();
        });
    }
    
    // 回到顶部按钮
    const backToTopBtn = document.getElementById('backToTop');
    window.addEventListener('scroll', function() {
        if (window.pageYOffset > 300) {
            backToTopBtn.style.display = 'block';
        } else {
            backToTopBtn.style.display = 'none';
        }
    });
    
    backToTopBtn.addEventListener('click', function() {
        window.scrollTo({top: 0, behavior: 'smooth'});
    });
}

// 加载页面数据 - 增加调试信息
function loadPageData(page) {
    const contentArea = document.getElementById('userContent');
    
    // 显示加载动画
    contentArea.innerHTML = '<div class="loading"><i class="fas fa-spinner fa-spin"></i> 正在加载...</div>';
    
    // 模拟AJAX请求
    setTimeout(() => {
        switch(page) {
            case 'profile':
                contentArea.innerHTML = generateProfileHTML();
                // 绑定修改密码按钮事件
                bindPasswordChangeBtn();
                // 加载个人资料页面后重新获取用户数据并更新页面
                fetch('/api/user/info')
                    .then(response => {
                        if (!response.ok) {
                            throw new Error('获取用户信息失败');
                        }
                        return response.json();
                    })
                    .then(userData => {
                        console.log('获取到的用户数据:', userData); // 添加调试信息
                        updateProfileContent(userData);
                    })
                    .catch(error => {
                        console.error('获取用户信息失败:', error);
                        showNotification('获取用户详细信息失败', 'error');
                    });
                break;
            case 'manage':
                contentArea.innerHTML = generateManageHTML();
                // 绑定博客管理相关事件
                bindManageEvents();
                break;
            case 'settings':
                contentArea.innerHTML = generateSettingsHTML();
                // 绑定设置表单事件
                bindSettingsEvents();
                // 获取用户数据填充表单
                fetch('/api/user/info')
                    .then(response => {
                        if (!response.ok) {
                            throw new Error('获取用户信息失败');
                        }
                        return response.json();
                    })
                    .then(userData => {
                        console.log('设置页面获取的用户数据:', userData); // 添加调试信息
                        updateSettingsForm(userData);
                    })
                    .catch(error => {
                        console.error('获取用户信息失败:', error);
                        showNotification('获取用户详细信息失败', 'error');
                    });
                break;
            case 'messages':
                contentArea.innerHTML = generateMessagesHTML();
                // 绑定消息中心相关事件
                bindMessagesEvents();
                break;
            default:
                contentArea.innerHTML = '<p>内容加载失败</p>';
        }
    }, 500);
}

// 生成个人资料HTML - 修改版，适配动态数据
function generateProfileHTML() {
    return `
        <h2>个人资料</h2>
        <div class="profile-info">
            <div class="profile-section">
                <h3>基本信息</h3>
                <div class="info-item">
                    <label>用户名</label>
                    <span>加载中...</span>
                </div>
                <div class="info-item">
                    <label>邮箱</label>
                    <span>加载中...</span>
                </div>
                <div class="info-item">
                    <label>注册时间</label>
                    <span>加载中...</span>
                </div>
                <div class="info-item">
                    <label>个人简介</label>
                    <p>加载中...</p>
                </div>
            </div>
            
            <div class="profile-section">
                <h3>博客统计</h3>
                <div class="profile-stats">
                    <div class="stat-item">
                        <div class="stat-value">0</div>
                        <div class="stat-label">发布博客</div>
                    </div>
                    <div class="stat-item">
                        <div class="stat-value">0</div>
                        <div class="stat-label">总浏览量</div>
                    </div>
                    <div class="stat-item">
                        <div class="stat-value">0</div>
                        <div class="stat-label">获赞数</div>
                    </div>
                    <div class="stat-item">
                        <div class="stat-value">0</div>
                        <div class="stat-label">评论数</div>
                    </div>
                </div>
            </div>
            
            <div class="profile-section">
                <h3>账户安全</h3>
                <button class="btn btn-primary" id="changePasswordBtn">修改密码</button>
            </div>
        </div>
    `;
}

// 生成博客管理HTML - 优化版
function generateManageHTML() {
    return `
        <h2><i class="fas fa-file-alt"></i> 博客管理</h2>
        <div class="manage-filter">
            <input type="text" class="search-input" placeholder="搜索博客标题或内容...">
            <select class="sort-select">
                <option value="postTime">时间排序</option>
                <option value="view_count">浏览量</option>
                <option value="like_count">点赞数</option>
            </select>
        </div>
        <div class="manage-list">
            <div class="loading">
                <i class="fas fa-spinner fa-spin"></i>
                <p>正在加载博客列表...</p>
            </div>
        </div>
    `;
}

// 生成设置页面HTML
function generateSettingsHTML() {
    return `
        <h2>账号设置</h2>
        <div class="settings-form">
            <form id="settingsForm">
                <div class="form-group">
                    <label for="username">用户名</label>
                    <div class="input-with-note">
                        <input type="text" id="username" value="" placeholder="请输入用户名" disabled>
                        <span class="note-text" style="color: #f39c12; margin-left: 10px;"><i class="fas fa-info-circle"></i> 暂无法修改用户名</span>
                    </div>
                </div>
                
                <div class="form-group">
                    <label for="email">邮箱</label>
                    <input type="email" id="email" value="" placeholder="请输入邮箱">
                    <small class="form-hint">请输入有效的电子邮箱地址</small>
                </div>
                
                <div class="form-group">
                    <label for="bio">个人简介</label>
                    <textarea id="bio" rows="5" placeholder="请输入个人简介" maxlength="200"></textarea>
                    <small class="form-hint">个人简介最多200字</small>
                </div>
                
                <div class="btn-group">
                    <button type="submit" class="btn btn-primary" id="saveSettingsBtn">保存更改</button>
                </div>
            </form>
        </div>
    `;
}

// 生成消息中心HTML
function generateMessagesHTML() {
    // 模拟消息数据
    const messages = [
        { id: 1, sender: '系统通知', content: '您的账号已成功注册，欢迎加入云中杉木博客平台！', time: '2023-10-15 14:30', read: true },
        { id: 2, sender: '评论通知', content: '用户"技术爱好者"评论了您的博客《JavaScript高级编程技巧》: "写得太好了，学到很多!"', time: '2023-10-14 09:15', read: false },
        { id: 3, sender: '点赞通知', content: '用户"前端达人"给您的博客《CSS Grid布局完全指南》点了赞', time: '2023-10-12 18:45', read: false },
        { id: 4, sender: '系统通知', content: '您的博客《Vue3和React18的异同点分析》已被推荐到首页', time: '2023-10-10 11:20', read: true },
    ];
    
    let html = `
        <h2>消息中心</h2>
        <div class="message-tabs">
            <div class="message-tab active" data-type="all">全部消息</div>
            <div class="message-tab" data-type="unread">未读消息</div>
            <div class="message-tab" data-type="system">系统通知</div>
            <div class="message-tab" data-type="comment">评论通知</div>
        </div>
        <div class="message-list">
    `;
    
    if (messages.length === 0) {
        html += `
            <div class="empty-message">
                <i class="far fa-bell"></i>
                <p>暂无消息</p>
            </div>
        `;
    } else {
        messages.forEach(msg => {
            html += `
                <div class="message-item ${msg.read ? '' : 'unread'}" data-id="${msg.id}">
                    <div class="message-header">
                        <div class="message-sender">${msg.sender}</div>
                        <div class="message-time">${msg.time}</div>
                    </div>
                    <div class="message-content">${msg.content}</div>
                </div>
            `;
        });
    }
    
    html += `
        </div>
    `;
    
    return html;
}

// 设置主题切换
function setupThemeToggle() {
    const themeToggle = document.getElementById('themeToggle');
    const body = document.body;
    
    themeToggle.addEventListener('click', function() {
        toggleDarkMode();
    });
}

// 检查主题设置
function checkThemeSetting() {
    const isDarkMode = localStorage.getItem('darkMode') === 'true';
    const themeToggle = document.getElementById('themeToggle');
    
    if (isDarkMode) {
        document.body.classList.add('dark-theme');
        updateDarkModeIcon(true);
    }
}

// 切换暗黑模式 - 新增函数
function toggleDarkMode() {
    const isDarkMode = document.body.classList.toggle('dark-theme');
    localStorage.setItem('darkMode', isDarkMode);
    updateDarkModeIcon(isDarkMode);
    showNotification(`已切换到${isDarkMode ? '暗色' : '亮色'}模式`, 'success');
}

// 更新暗黑模式图标 - 新增函数
function updateDarkModeIcon(isDarkMode) {
    const themeToggle = document.getElementById('themeToggle');
    if (themeToggle) {
        themeToggle.innerHTML = isDarkMode ? 
            '<i class="fas fa-sun"></i>' : 
            '<i class="fas fa-moon"></i>';
    }
}

// 移动菜单初始化
function initMobileMenu() {
    const mobileToggle = document.querySelector('.mobile-toggle');
    const mainNav = document.querySelector('.main-nav');
    
    mobileToggle.addEventListener('click', function() {
        mainNav.classList.toggle('show-mobile');
        mobileToggle.classList.toggle('active');
    });
}

// 绑定博客管理事件
function bindManageEvents() {
    // 获取URL参数
    const searchParams = new URLSearchParams(window.location.search);
    let page = 1;
    let keyword = '';
    let sort = 'postTime';
    
    if (searchParams.has('page')) {
        page = parseInt(searchParams.get('page'));
    }
    
    if (searchParams.has('keyword')) {
        keyword = searchParams.get('keyword');
    }
    
    if (searchParams.has('sort')) {
        sort = searchParams.get('sort');
    }
    
    // 设置搜索和排序控件的初始值
    const searchInput = document.querySelector('.search-input');
    const sortSelect = document.querySelector('.sort-select');
    
    if (searchInput && keyword) {
        searchInput.value = keyword;
    }
    
    if (sortSelect && sort) {
        sortSelect.value = sort;
    }
    
    // 绑定搜索输入框事件 - 使用防抖
    if (searchInput) {
        searchInput.addEventListener('input', debounce(function() {
            const keyword = this.value.trim();
            const sort = sortSelect ? sortSelect.value : 'postTime';
            
            // 请求数据，重置到第一页
            fetchUserBlogs(keyword, sort, 1, 6); // 搜索时重置到第一页
            
            // 更新URL参数，确保页码为1
            const url = new URL(window.location.href);
            url.searchParams.set('page', 1); // 确保页码重置为1
            
            // 只有当关键词非空时添加到URL
            if (keyword) {
                url.searchParams.set('keyword', keyword);
                // 搜索关键词变更时显示通知
                showNotification(`正在搜索: "${keyword}"`, 'info');
            } else {
                url.searchParams.delete('keyword');
                if (url.searchParams.has('keyword')) {
                    // 清空搜索时显示通知
                    showNotification('已清空搜索条件', 'info');
                }
            }
            
            // 保持排序参数
            if (sort !== 'postTime') {
                url.searchParams.set('sort', sort);
            } else {
                url.searchParams.delete('sort');
            }
            
            window.history.pushState({}, '', url);
        }, 500));
    }
    
    // 绑定排序下拉框事件
    if (sortSelect) {
        sortSelect.addEventListener('change', function() {
            const keyword = searchInput ? searchInput.value.trim() : '';
            const sort = this.value;
            
            // 显示排序方式变更通知
            showNotification(`排序方式已更改为: ${getSortDisplayName(sort)}`, 'info');
            
            // 请求数据，重置到第一页
            fetchUserBlogs(keyword, sort, 1, 6);
            
            // 更新URL参数，确保页码为1
            const url = new URL(window.location.href);
            url.searchParams.set('page', 1); // 确保页码重置为1
            url.searchParams.set('sort', sort);
            if (keyword) {
                url.searchParams.set('keyword', keyword);
            } else {
                url.searchParams.delete('keyword');
            }
            window.history.pushState({}, '', url);
        });
    }
    
    // 加载博客列表
    fetchUserBlogs(keyword, sort, page, 6);
}

// 获取排序方式的显示名称
function getSortDisplayName(sortValue) {
    switch(sortValue) {
        case 'postTime':
            return '最新发布';
        case 'view_count':
            return '最多浏览';
        case 'like_count':
            return '最多点赞';
        default:
            return sortValue;
    }
}

// 绑定设置表单事件
function bindSettingsEvents() {
    const settingsForm = document.getElementById('settingsForm');
    const saveBtn = document.getElementById('saveSettingsBtn');
    
    // 保存设置
    settingsForm.addEventListener('submit', function(e) {
        e.preventDefault();
        
        const email = document.getElementById('email').value;
        const bio = document.getElementById('bio').value;
        
        // 表单验证
        if (!email) {
            showNotification('请输入电子邮箱', 'error');
            return;
        }
        
        if (!validateEmail(email)) {
            showNotification('请输入有效的电子邮箱地址', 'error');
            return;
        }
        
        if (bio && bio.length > 200) {
            showNotification('个人简介不能超过200字', 'error');
            return;
        }
        
        // 显示保存中状态
        saveBtn.disabled = true;
        saveBtn.innerHTML = '<i class="fas fa-spinner fa-spin"></i> 保存中...';
        
        const formData = {
            // 不包含用户名，因为不允许修改
            email: email,
            bio: bio
        };
        
        // 发送更新请求
        updateUserSettings(formData);
    });
}

// 邮箱验证函数
function validateEmail(email) {
    const re = /^(([^<>()\[\]\\.,;:\s@"]+(\.[^<>()\[\]\\.,;:\s@"]+)*)|(".+"))@((\[[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\])|(([a-zA-Z\-0-9]+\.)+[a-zA-Z]{2,}))$/;
    return re.test(String(email).toLowerCase());
}

// 更新updateUserSettings函数以恢复按钮状态
function updateUserSettings(formData) {
    // 显示保存中提示
    showNotification('保存中...', 'info');
    const saveBtn = document.getElementById('saveSettingsBtn');
    
    // 发送请求
    fetch('/api/user/update', {
        method: 'PATCH',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify(formData)
    })
    .then(response => {
        if (!response.ok) {
            throw new Error('保存失败');
        }
        return response.json();
    })
    .then(data => {
        showNotification('设置已保存', 'success');
        
        // 更新用户信息显示
        document.getElementById('userName').textContent = formData.username;
        
        // 恢复按钮状态
        if (saveBtn) {
            saveBtn.disabled = false;
            saveBtn.innerHTML = '保存更改';
        }
    })
    .catch(error => {
        showNotification('保存失败：' + error.message, 'error');
        
        // 恢复按钮状态
        if (saveBtn) {
            saveBtn.disabled = false;
            saveBtn.innerHTML = '保存更改';
        }
    });
}

// 绑定消息中心事件
function bindMessagesEvents() {
    // 消息标签切换
    const messageTabs = document.querySelectorAll('.message-tab');
    messageTabs.forEach(tab => {
        tab.addEventListener('click', function() {
            messageTabs.forEach(t => t.classList.remove('active'));
            this.classList.add('active');
            
            const type = this.getAttribute('data-type');
            filterMessages(type);
        });
    });
    
    // 点击消息标记为已读
    const messageItems = document.querySelectorAll('.message-item');
    messageItems.forEach(item => {
        item.addEventListener('click', function() {
            const messageId = this.getAttribute('data-id');
            this.classList.remove('unread');
            
            // 发送标记已读请求
            markMessageAsRead(messageId);
        });
    });
}

// 过滤消息
function filterMessages(type) {
    const messageItems = document.querySelectorAll('.message-item');
    
    messageItems.forEach(item => {
        if (type === 'all') {
            item.style.display = 'block';
        } else if (type === 'unread') {
            if (item.classList.contains('unread')) {
                item.style.display = 'block';
            } else {
                item.style.display = 'none';
            }
        } else if (type === 'system' || type === 'comment') {
            const sender = item.querySelector('.message-sender').textContent.toLowerCase();
            if (sender.includes(type)) {
                item.style.display = 'block';
            } else {
                item.style.display = 'none';
            }
        }
    });
    
    // 检查是否有消息显示
    const visibleMessages = Array.from(messageItems).filter(item => item.style.display !== 'none');
    const messageList = document.querySelector('.message-list');
    
    if (visibleMessages.length === 0) {
        // 没有匹配的消息，显示空状态
        const emptyMessage = document.createElement('div');
        emptyMessage.className = 'empty-message';
        emptyMessage.innerHTML = `
            <i class="far fa-bell"></i>
            <p>暂无${type === 'all' ? '' : type === 'unread' ? '未读' : type === 'system' ? '系统' : '评论'}消息</p>
        `;
        
        // 删除可能存在的空状态提示
        const existingEmpty = messageList.querySelector('.empty-message');
        if (existingEmpty) {
            existingEmpty.remove();
        }
        
        messageList.appendChild(emptyMessage);
    } else {
        // 有匹配消息，删除可能存在的空状态提示
        const existingEmpty = messageList.querySelector('.empty-message');
        if (existingEmpty) {
            existingEmpty.remove();
        }
    }
}

// 上传头像到服务器后更新所有头像显示
function uploadAvatarToServer(file) {
    // 创建FormData对象
    const formData = new FormData();
    formData.append('avatar', file);
    
    // 显示上传中提示
    showNotification('头像上传中...', 'info');
    
    // 发送请求
    fetch('/api/user/avatar', {
        method: 'POST',
        body: formData
    })
    .then(response => {
        if (!response.ok) {
            throw new Error('上传失败');
        }
        return response.json();
    })
    .then(data => {
        showNotification('头像上传成功', 'success');
        
        // 如果API返回了新的头像URL，使用它更新所有头像
        if (data.avatarUrl) {
            document.getElementById('userAvatar').src = data.avatarUrl;
            document.getElementById('avatarImage').src = data.avatarUrl;
        }
    })
    .catch(error => {
        showNotification('头像上传失败：' + error.message, 'error');
    });
}

// 获取用户博客列表 - 新增函数
function fetchUserBlogs(keyword = '', sort = 'postTime', page = 1, pageSize = 6) {
    // 构建查询参数
    const params = new URLSearchParams({
        keyword: keyword,
        sort: sort,
        page: page,
        pageSize: pageSize
    });
    
    // 显示加载状态
    const contentArea = document.querySelector('.manage-list');
    if (contentArea) {
        // 先添加淡出效果
        contentArea.classList.add('content-fading');
        
        // 在同一次事件循环中标记正在加载，避免重复调用
        contentArea.setAttribute('data-loading', 'true');
        
        // 短暂延迟后再显示加载状态
        setTimeout(() => {
            // 检查是否仍在加载中（避免快速切换导致的闪烁）
            if (contentArea.getAttribute('data-loading') === 'true') {
                contentArea.innerHTML = '<div class="loading"><i class="fas fa-spinner fa-spin"></i> 加载中...</div>';
                contentArea.classList.remove('content-fading');
                contentArea.classList.add('content-loading');
            }
        }, 300);
    }
    
    // 发送请求
    fetch(`/api/blogs/user?${params.toString()}`)
        .then(response => {
            if (!response.ok) {
                throw new Error('获取博客列表失败');
            }
            return response.text(); // 先获取文本响应
        })
        .then(text => {
            try {
                // 尝试解析JSON并处理可能的控制字符
                // 移除不可打印字符
                const cleanText = text.replace(/[\x00-\x1F\x7F-\x9F]/g, '');
                return JSON.parse(cleanText);
            } catch (error) {
                console.error('JSON解析失败:', error);
                console.log('原始响应文本:', text);
                throw new Error('解析博客数据失败');
            }
        })
        .then(data => {
            // 移除加载标记
            if (contentArea) {
                contentArea.removeAttribute('data-loading');
                contentArea.classList.remove('content-loading');
                contentArea.classList.remove('content-fading');
            }
            
            // 更新UI
            updateBlogList(data.data.blogs, data.data.total);
            
            // 如果当前URL中的页码超出了实际页数，修正URL
            const totalPages = Math.ceil(data.data.total / pageSize);
            const currentUrlParams = new URLSearchParams(window.location.search);
            const currentPage = currentUrlParams.has('page') ? parseInt(currentUrlParams.get('page')) : 1;
            
            if (currentPage > totalPages && totalPages > 0) {
                // 更新URL参数为最后一页
                const url = new URL(window.location.href);
                url.searchParams.set('page', totalPages);
                window.history.replaceState({}, '', url);
                
                // 如果不是搜索导致的自动跳转，显示通知
                if (!keyword) {
                    showNotification(`已自动跳转到最后一页: ${totalPages}`, 'info');
                }
            }
            
            // 添加内容进入动画
            setTimeout(() => {
                const blogCards = document.querySelectorAll('.manage-card');
                blogCards.forEach((card, index) => {
                    setTimeout(() => {
                        card.classList.add('card-visible');
                    }, index * 100); // 每个卡片错开显示
                });
            }, 100);
        })
        .catch(error => {
            console.error('获取博客列表失败:', error);
            if (contentArea) {
                contentArea.removeAttribute('data-loading');
                contentArea.classList.remove('content-loading');
                contentArea.classList.remove('content-fading');
                contentArea.innerHTML = '<div class="empty-message"><i class="fas fa-exclamation-circle"></i><p>加载失败，请稍后重试</p></div>';
            }
            showNotification('获取博客列表失败，请稍后重试', 'error');
        });
}

// 完整的更新博客列表函数
function updateBlogList(blogs, total) {
    const contentArea = document.querySelector('.manage-list');
    if (!contentArea) return;
    
    // 确保不在已加载页面上重新显示加载状态
    if (contentArea.getAttribute('data-loading') === 'true') {
        return;
    }
    
    // 清空内容区域
    contentArea.innerHTML = '';
    
    // 检查是否有博客
    if (!blogs || blogs.length === 0) {
        contentArea.innerHTML = '<div class="empty-message"><i class="far fa-file-alt"></i><p>暂无博客内容</p></div>';
        return;
    }
    
    // 设置列表布局
    contentArea.style.display = 'grid';
    contentArea.style.gridTemplateColumns = 'repeat(auto-fill, minmax(280px, 1fr))';
    contentArea.style.gap = '20px';
    
    // 创建一个文档片段来批量添加DOM元素，提高性能
    const fragment = document.createDocumentFragment();
    
    // 渲染博客列表
    blogs.forEach(blog => {
        try {
            const statusText = blog.status === 'published' ? '已发布' : '草稿';
            const statusClass = blog.status === 'published' ? 'published' : 'draft';
            
            // 判断图片是否存在且有效
            let hasCoverImage = blog.coverImage && blog.coverImage.trim() !== '';
            
            const blogCard = document.createElement('div');
            blogCard.className = 'manage-card'; // 初始无可见性类，由动画添加
            blogCard.dataset.blogId = blog.id;
            
            blogCard.innerHTML = `
                <div class="blog-status ${statusClass}">${statusText}</div>
                <div class="blog-thumbnail">
                    <img src="${hasCoverImage ? blog.coverImage : 'img/default_touxiang.jpg'}" alt="博客封面" onerror="this.src='img/default_touxiang.jpg'">
                </div>
                <div class="blog-info">
                    <h3 class="blog-title"><a href="blog_detail.html?id=${blog.id}" title="${blog.title}">${blog.title}</a></h3>
                    <div class="blog-meta">
                        <span><i class="far fa-calendar-alt"></i> ${new Date(blog.postTime || blog.publishDate).toLocaleDateString()}</span>
                        <span><i class="far fa-eye"></i> ${blog.views || 0}</span>
                        <span><i class="far fa-thumbs-up"></i> ${blog.likes || 0}</span>
                    </div>
                    <div class="blog-actions">
                        <button class="edit-btn" data-blog-id="${blog.id}"><i class="far fa-edit"></i> 编辑</button>
                        <button class="delete-btn" data-blog-id="${blog.id}"><i class="far fa-trash-alt"></i> 删除</button>
                    </div>
                </div>
            `;
            
            fragment.appendChild(blogCard);
        } catch (error) {
            console.error('渲染博客卡片出错:', error, blog);
        }
    });
    
    // 一次性将所有卡片添加到DOM中
    contentArea.appendChild(fragment);
    
    // 绑定博客卡片事件
    bindBlogCardEvents();
    
    // 添加分页
    addPagination(total, 6);
}

// 绑定博客卡片事件
function bindBlogCardEvents() {
    // 绑定编辑按钮事件
    document.querySelectorAll('.edit-btn').forEach(btn => {
        btn.addEventListener('click', function() {
            const blogId = this.dataset.blogId;
            if (blogId) {
                // 直接调用editBlog函数处理
                editBlog(blogId);
            } else {
                showNotification('无效的博客ID', 'error');
            }
        });
    });
    
    // 绑定删除按钮事件
    document.querySelectorAll('.delete-btn').forEach(btn => {
        btn.addEventListener('click', function() {
            const blogId = this.dataset.blogId;
            confirmDeleteBlog(blogId);
        });
    });
}

// 获取博客数据并打开编辑器
async function fetchBlogDataAndOpenEditor(blogId) {
    try {
        showNotification('正在加载博客数据...', 'info');
        
        const response = await fetch(`/api/blogs/${blogId}`);
        if (!response.ok) {
            throw new Error('获取博客数据失败');
        }
        
        let blogData = await response.json();
        
        // 处理不同格式的API响应
        if (blogData.code && blogData.data) {
            blogData = blogData.data;
        }
        
        // 确保统一封面图片字段名
        if (!blogData.coverImage) {
            // 尝试其他可能的字段名
            blogData.coverImage = blogData.cover_image || blogData.thumbnail || blogData.cover || '';
            console.log('统一封面图片字段:', blogData.coverImage);
        }
        
        // 创建编辑器模态框
        showBlogEditor(blogData);
    } catch (error) {
        console.error('加载博客数据失败:', error);
        showNotification('加载博客数据失败，请重试', 'error');
    }
}

// 显示博客编辑器
function showBlogEditor(blogData) {
    // 创建模态框HTML
    const modalHtml = `
        <div class="modal-overlay" id="blogEditorModal">
            <div class="modal-container" style="max-width: 90%; width: 1000px;">
                <div class="modal-header">
                    <h3>编辑博客</h3>
                    <button class="modal-close-btn" id="modalCloseBtn"><i class="fas fa-times"></i></button>
                </div>
                <div class="modal-body">
                    <form id="blogEditForm">
                        <div class="form-group">
                            <label for="blogTitle">文章标题</label>
                            <input type="text" id="blogTitle" name="title" value="${blogData.title || ''}" required>
                        </div>
                        
                        <div class="form-group">
                            <label for="editor">文章内容</label>
                            <div id="editor" class="editor-wrapper"></div>
                        </div>
                        
                        <div class="form-group">
                            <label for="blogCategory">分类</label>
                            <select id="blogCategory" name="category">
                                <option value="">选择分类</option>
                            </select>
                        </div>
                        
                        <div class="form-group">
                            <label for="tagsInput">标签</label>
                            <div class="tags-input" id="tagsContainer">
                                <input type="text" id="tagsInput" placeholder="输入标签后按回车添加" />
                            </div>
                            <small class="form-hint">最多添加5个标签，每个标签不超过10个字符</small>
                        </div>
                        
                        <div class="form-group">
                            <label for="thumbnailUpload">封面图片</label>
                            <input type="file" id="thumbnailUpload" name="thumbnail" accept="image/*">
                            <div class="thumbnail-preview" id="thumbnailPreview">
                                ${blogData.coverImage ? `<img src="${blogData.coverImage}" alt="封面预览">` : '<span>尚未上传封面图片</span>'}
                            </div>
                        </div>
                    </form>
                </div>
                <div class="modal-footer">
                    <button class="btn btn-secondary" id="cancelEditBtn">取消</button>
                    <button class="btn btn-primary" id="saveEditBtn">保存更改</button>
                </div>
            </div>
        </div>
    `;
    
    // 添加模态框到页面
    document.body.insertAdjacentHTML('beforeend', modalHtml);
    
    // 初始化编辑器
    initBlogEditor(blogData);
    
    // 显示模态框
    const modal = document.getElementById('blogEditorModal');
    setTimeout(() => {
        modal.classList.add('show');
    }, 10);
    
    // 绑定关闭事件
    const closeBtn = document.getElementById('modalCloseBtn');
    const cancelBtn = document.getElementById('cancelEditBtn');
    
    function closeModal() {
        modal.classList.remove('show');
        setTimeout(() => {
            modal.remove();
        }, 300);
    }
    
    closeBtn.addEventListener('click', closeModal);
    cancelBtn.addEventListener('click', closeModal);
    
    // 点击模态框外部关闭
    modal.addEventListener('click', function(e) {
        if (e.target === modal) {
            closeModal();
        }
    });
    
    // 绑定保存事件
    const saveBtn = document.getElementById('saveEditBtn');
    saveBtn.addEventListener('click', () => saveBlogEdit(blogData.id, closeModal));
}

// 初始化博客编辑器 - 完善版
function initBlogEditor(blogData) {
    // 初始化Quill编辑器
    const quill = new Quill('#editor', {
        theme: 'snow',
        modules: {
            toolbar: [
                [{ 'header': [1, 2, 3, 4, 5, 6, false] }],
                ['bold', 'italic', 'underline', 'strike'],
                [{ 'color': [] }, { 'background': [] }],
                [{ 'align': [] }],
                ['blockquote', 'code-block'],
                [{ 'list': 'ordered'}, { 'list': 'bullet' }],
                ['link', 'image'],
                ['clean']
            ]
        }
    });
    
    // 设置编辑器内容
    if (blogData.content) {
        // 如果内容是Markdown格式，先转换为HTML
        if (blogData.content_format === 'markdown') {
            const html = marked.parse(blogData.content);
            quill.root.innerHTML = html;
        } else {
            quill.root.innerHTML = blogData.content;
        }
    }
    
    // 加载分类
    loadCategories(blogData.category);
    
    // 初始化标签输入
    initTagsInput(blogData.tags);
    
    // 图片上传处理
    initImageUpload();
}

// 初始化标签输入功能
function initTagsInput(existingTags = []) {
    const tagsContainer = document.getElementById('tagsContainer');
    const tagsInput = document.getElementById('tagsInput');
    
    // 清空现有标签
    Array.from(tagsContainer.querySelectorAll('.tag')).forEach(tag => tag.remove());
    
    // 添加已有标签
    if (Array.isArray(existingTags)) {
        existingTags.forEach(tag => addTag(tag));
    } else if (typeof existingTags === 'string') {
        // 处理字符串格式的标签 - 可能是JSON字符串或逗号分隔的字符串
        try {
            // 尝试解析JSON
            const tagsArray = JSON.parse(existingTags);
            if (Array.isArray(tagsArray)) {
                tagsArray.forEach(tag => addTag(tag));
            }
        } catch (e) {
            // 如果不是有效的JSON，假设是逗号分隔的字符串
            const tagsArray = existingTags.split(',').map(tag => tag.trim()).filter(tag => tag);
            tagsArray.forEach(tag => addTag(tag));
        }
    }
    
    // 绑定标签输入事件
    tagsInput.addEventListener('keydown', function(e) {
        if (e.key === 'Enter' || e.key === ',') {
            e.preventDefault();
            
            const tagValue = this.value.trim();
            if (tagValue) {
                if (tagValue.length > 10) {
                    showNotification('标签长度不能超过10个字符', 'error');
                    return;
                }
                
                const existingTags = document.querySelectorAll('.tag');
                if (existingTags.length >= 5) {
                    showNotification('最多添加5个标签', 'error');
                    return;
                }
                
                addTag(tagValue);
                this.value = '';
            }
        }
    });
}

// 初始化图片上传功能
function initImageUpload() {
    const thumbnailUpload = document.getElementById('thumbnailUpload');
    const thumbnailPreview = document.getElementById('thumbnailPreview');
    
    // 确保初始状态是正确的 - 如果已经有图片，移除no-image类
    if (thumbnailPreview.querySelector('img')) {
        thumbnailPreview.classList.remove('no-image');
    } else {
        thumbnailPreview.classList.add('no-image');
    }
    
    thumbnailUpload.addEventListener('change', function(e) {
        const file = this.files[0];
        if (file) {
            // 验证文件类型
            if (!file.type.startsWith('image/')) {
                showNotification('请选择图片文件', 'error');
                this.value = '';
                return;
            }
            
            // 验证文件大小（限制为2MB）
            if (file.size > 2 * 1024 * 1024) {
                showNotification('图片大小不能超过2MB', 'error');
                this.value = '';
                return;
            }
            
            const reader = new FileReader();
            reader.onload = function(e) {
                thumbnailPreview.innerHTML = `<img src="${e.target.result}" alt="封面预览">`;
                thumbnailPreview.classList.remove('no-image');
            };
            reader.readAsDataURL(file);
        }
    });
    
    // 支持拖放上传
    thumbnailPreview.addEventListener('dragover', function(e) {
        e.preventDefault();
        e.stopPropagation();
        this.style.borderColor = 'var(--primary-color)';
    });
    
    thumbnailPreview.addEventListener('dragleave', function(e) {
        e.preventDefault();
        e.stopPropagation();
        this.style.borderColor = 'var(--border-color)';
    });
    
    thumbnailPreview.addEventListener('drop', function(e) {
        e.preventDefault();
        e.stopPropagation();
        this.style.borderColor = 'var(--border-color)';
        
        const file = e.dataTransfer.files[0];
        if (file) {
            thumbnailUpload.files = e.dataTransfer.files;
            const event = new Event('change');
            thumbnailUpload.dispatchEvent(event);
        }
    });
}

// 添加标签函数 - 完善版
function addTag(tagValue) {
    const tagsContainer = document.getElementById('tagsContainer');
    const tagsInput = document.getElementById('tagsInput');
    const existingTags = document.querySelectorAll('.tag');
    
    // 去除可能存在的×符号
    let cleanTagValue = tagValue.replace(/\s*×\s*$/, '').trim();
    
    // 检查是否已存在相同标签
    const isDuplicate = Array.from(existingTags).some(tag => {
        // 清除标签中的×符号再进行比较
        let existingTagText = tag.textContent.replace(/\s*×\s*$/, '').trim();
        return existingTagText === cleanTagValue;
    });
    
    if (isDuplicate) {
        showNotification('该标签已存在', 'error');
        return;
    }
    
    if (existingTags.length >= 5) {
        showNotification('最多添加5个标签', 'error');
        return;
    }
    
    const tagElement = document.createElement('span');
    tagElement.className = 'tag';
    tagElement.innerHTML = `${cleanTagValue} <span class="remove-tag">&times;</span>`;
    
    const removeBtn = tagElement.querySelector('.remove-tag');
    removeBtn.addEventListener('click', function() {
        tagElement.remove();
    });
    
    tagsContainer.insertBefore(tagElement, tagsInput);
}

// 保存博客编辑
async function saveBlogEdit(blogId, closeModal) {
    try {
        const title = document.getElementById('blogTitle').value;
        const content = document.querySelector('.ql-editor').innerHTML;
        const category = document.getElementById('blogCategory').value;
        
        // 获取标签并清理×符号
        const tags = Array.from(document.querySelectorAll('.tag')).map(tag => {
            return tag.textContent.replace(/\s*×\s*$/, '').trim();
        }).filter(tag => tag); // 过滤掉空标签
        
        if (!title) {
            showNotification('请输入文章标题', 'error');
            return;
        }
        
        // 显示保存中状态
        const saveBtn = document.getElementById('saveEditBtn');
        saveBtn.disabled = true;
        saveBtn.innerHTML = '<i class="fas fa-spinner fa-spin"></i> 保存中...';
        
        // 准备表单数据
        const formData = new FormData();
        formData.append('title', title);
        formData.append('content', content);
        formData.append('category', category);
        formData.append('tags', JSON.stringify(tags));
        
        console.log('保存的标签:', tags); // 调试日志
        
        // 处理封面图片
        const thumbnailFile = document.getElementById('thumbnailUpload').files[0];
        let hasCoverImage = false;
        
        if (thumbnailFile) {
            // 如果有新选择的图片，上传新图片
            formData.append('thumbnail', thumbnailFile);
            hasCoverImage = true;
        } else {
            // 如果没有新选择的图片，但有现有的封面图片，传递原有的URL
            const thumbnailPreview = document.getElementById('thumbnailPreview');
            const thumbnailImg = thumbnailPreview.querySelector('img');
            if (thumbnailImg && thumbnailImg.src) {
                try {
                    // 获取相对路径 - 从URL中提取
                    let coverImageUrl = thumbnailImg.src;
                    
                    // 处理绝对URL转为相对路径
                    if (coverImageUrl.includes('/thumbnail/')) {
                        const pathMatch = coverImageUrl.match(/\/thumbnail\/[^?#]*/);
                        if (pathMatch) {
                            coverImageUrl = pathMatch[0];
                        }
                    }
                    
                    // 如果是数据URL (base64)，我们需要上传该图像
                    if (coverImageUrl.startsWith('data:image')) {
                        formData.append('thumbnailBase64', coverImageUrl);
                    } else {
                        formData.append('thumbnail_path', coverImageUrl);
                    }
                    
                    hasCoverImage = true;
                    console.log('保留原有封面图片:', coverImageUrl);
                } catch (error) {
                    console.error('处理封面图片URL失败:', error);
                }
            }
        }
        
        // 如果没有封面图片，明确指出
        if (!hasCoverImage) {
            formData.append('remove_thumbnail', 'true');
        }
        
        // 发送更新请求
        const response = await fetch(`/api/blogs/${blogId}`, {
            method: 'PATCH',
            body: formData
        });
        
        if (!response.ok) {
            throw new Error('更新博客失败');
        }
        
        const result = await response.json();
        
        showNotification('博客更新成功', 'success');
        closeModal();
        
        // 刷新博客列表
        fetchUserBlogs('', 'postTime', 1, 6);
        
    } catch (error) {
        console.error('保存博客失败:', error);
        showNotification('保存失败，请重试', 'error');
        
        // 恢复保存按钮状态
        const saveBtn = document.getElementById('saveEditBtn');
        saveBtn.disabled = false;
        saveBtn.textContent = '保存更改';
    }
}

// 确认删除博客 - 新增函数
function confirmDeleteBlog(blogId) {
    // 创建确认对话框
    const dialogHTML = `
        <div class="modal-overlay" id="deleteConfirmModal">
            <div class="modal-container">
                <div class="modal-header">
                    <h3>确认删除</h3>
                    <button class="modal-close-btn" id="modalCloseBtn"><i class="fas fa-times"></i></button>
                </div>
                <div class="modal-body">
                    <p class="confirm-message">确定要删除这篇博客吗？此操作不可恢复！</p>
                    <div class="btn-group">
                        <button class="btn btn-secondary" id="cancelDeleteBtn">取消</button>
                        <button class="btn btn-danger" id="confirmDeleteBtn">确认删除</button>
                    </div>
                </div>
            </div>
        </div>
    `;
    
    // 添加对话框到页面
    document.body.insertAdjacentHTML('beforeend', dialogHTML);
    
    // 显示对话框
    const modal = document.getElementById('deleteConfirmModal');
    setTimeout(() => {
        modal.classList.add('show');
    }, 10);
    
    // 绑定关闭按钮
    document.getElementById('modalCloseBtn').addEventListener('click', () => {
        closeModal(modal);
    });
    
    // 绑定取消按钮
    document.getElementById('cancelDeleteBtn').addEventListener('click', () => {
        closeModal(modal);
    });
    
    // 点击对话框外部关闭
    modal.addEventListener('click', function(e) {
        if (e.target === modal) {
            closeModal(modal);
        }
    });
    
    // 绑定确认删除按钮
    document.getElementById('confirmDeleteBtn').addEventListener('click', () => {
        // 执行删除操作
        deleteBlog(blogId, modal);
    });
}

// 关闭模态框函数 - 新增函数
function closeModal(modal) {
    modal.classList.add('closing');
    setTimeout(() => {
        modal.remove();
    }, 300);
}

// 删除博客 - 修改版
function deleteBlog(blogId, modal) {
    // 更新删除按钮状态
    const confirmBtn = document.getElementById('confirmDeleteBtn');
    confirmBtn.disabled = true;
    confirmBtn.innerHTML = '<i class="fas fa-spinner fa-spin"></i> 删除中...';
    
    // 发送删除请求
    fetch(`/api/blogs/${blogId}`, {
        method: 'DELETE',
        headers: {
            'Content-Type': 'application/json'
        }
    })
    .then(response => {
        if (!response.ok) {
            throw new Error('删除失败');
        }
        return response.json();
    })
    .then(data => {
        // 关闭对话框
        closeModal(modal);
        
        // 显示成功消息
        showNotification(data.message || '博客已成功删除', 'success');
        
        // 移除对应的博客卡片
        const blogCard = document.querySelector(`.manage-card[data-blog-id="${blogId}"]`);
        if (blogCard) {
            blogCard.classList.add('card-removed');
            setTimeout(() => {
                blogCard.remove();
                
                // 检查是否还有博客卡片
                const remainingCards = document.querySelectorAll('.manage-card');
                if (remainingCards.length === 0) {
                    document.querySelector('.manage-list').innerHTML = '<div class="empty-message"><i class="far fa-file-alt"></i><p>暂无博客内容</p></div>';
                }
            }, 300);
        }
    })
    .catch(error => {
        console.error('删除博客失败:', error);
        
        // 恢复按钮状态
        confirmBtn.disabled = false;
        confirmBtn.innerHTML = '确认删除';
        
        // 显示错误消息
        showNotification('删除博客失败，请稍后重试', 'error');
    });
}

// 添加分页功能
function addPagination(total, pageSize) {
    const totalPages = Math.ceil(total / pageSize);
    
    // 移除旧的分页容器
    const oldPagination = document.querySelector('.pagination-container');
    if (oldPagination) {
        oldPagination.remove();
    }
    
    // 如果总页数小于等于1，不显示分页
    if (totalPages <= 1) return;
    
    // 创建新的分页容器
    const paginationContainer = document.createElement('div');
    paginationContainer.className = 'pagination-container';
    // 存储总数据量用于页面切换时计算总页数
    paginationContainer.dataset.total = total.toString();
    
    // 获取当前页码
    let currentPage = 1;
    const searchParams = new URLSearchParams(window.location.search);
    if (searchParams.has('page')) {
        currentPage = parseInt(searchParams.get('page'));
    }
    
    // 确保当前页码不超过总页数
    if (currentPage > totalPages) {
        currentPage = totalPages;
    }
    
    // 创建分页HTML
    let paginationHTML = '<div class="pagination">';
    
    // 上一页按钮
    paginationHTML += `
        <button class="pagination-btn prev-btn" ${currentPage <= 1 ? 'disabled' : ''}>
            <i class="fas fa-chevron-left"></i> 上一页
        </button>
        <div class="page-numbers">
    `;
    
    // 页码按钮
    const maxVisiblePages = 5;
    let startPage = Math.max(1, currentPage - Math.floor(maxVisiblePages / 2));
    let endPage = Math.min(totalPages, startPage + maxVisiblePages - 1);
    
    if (endPage - startPage + 1 < maxVisiblePages) {
        startPage = Math.max(1, endPage - maxVisiblePages + 1);
    }
    
    // 第一页
    if (startPage > 1) {
        paginationHTML += `<button class="page-btn" data-page="1">1</button>`;
        if (startPage > 2) {
            paginationHTML += `<span class="page-btn page-ellipsis">...</span>`;
        }
    }
    
    // 中间的页码
    for (let i = startPage; i <= endPage; i++) {
        paginationHTML += `<button class="page-btn ${i === currentPage ? 'active' : ''}" data-page="${i}">${i}</button>`;
    }
    
    // 最后一页
    if (endPage < totalPages) {
        if (endPage < totalPages - 1) {
            paginationHTML += `<span class="page-btn page-ellipsis">...</span>`;
        }
        paginationHTML += `<button class="page-btn" data-page="${totalPages}">${totalPages}</button>`;
    }
    
    paginationHTML += `
        </div>
        <button class="pagination-btn next-btn" ${currentPage >= totalPages ? 'disabled' : ''}>
            下一页 <i class="fas fa-chevron-right"></i>
        </button>
    </div>`;
    
    // 显示页面信息
    paginationHTML += `
        <div class="pagination-info">
            共 <span class="total-count">${total}</span> 篇博客，
            <span class="current-page">${currentPage}</span> / <span class="total-pages">${totalPages}</span> 页
        </div>
    `;
    
    // 添加分页HTML到容器
    paginationContainer.innerHTML = paginationHTML;
    
    // 添加到内容区
    const contentArea = document.querySelector('.user-content');
    contentArea.appendChild(paginationContainer);
    
    // 为分页容器添加淡入动画
    setTimeout(() => {
        paginationContainer.classList.add('pagination-visible');
    }, 300);
    
    // 绑定分页事件
    const pageButtons = paginationContainer.querySelectorAll('.page-btn:not(.page-ellipsis)');
    pageButtons.forEach(btn => {
        btn.addEventListener('click', function() {
            // 添加点击反馈动画
            this.classList.add('btn-pulse');
            
            const page = this.dataset.page;
            if (page) {
                setTimeout(() => {
                    goToPage(parseInt(page));
                }, 150); // 短暂延迟，让按钮动画有时间显示
            }
        });
    });
    
    // 绑定上一页、下一页按钮事件
    const prevBtn = paginationContainer.querySelector('.prev-btn');
    const nextBtn = paginationContainer.querySelector('.next-btn');
    
    if (prevBtn && !prevBtn.disabled) {
        prevBtn.addEventListener('click', () => {
            prevBtn.classList.add('btn-pulse');
            setTimeout(() => {
                goToPage(currentPage - 1);
            }, 150);
        });
    }
    
    if (nextBtn && !nextBtn.disabled) {
        nextBtn.addEventListener('click', () => {
            nextBtn.classList.add('btn-pulse');
            setTimeout(() => {
                goToPage(currentPage + 1);
            }, 150);
        });
    }
    
    // 跳转到指定页面
    function goToPage(pageNum) {
        // 获取当前的搜索和排序参数
        const searchInput = document.querySelector('.search-input');
        const sortSelect = document.querySelector('.sort-select');
        
        let keyword = '';
        let sort = 'postTime';
        
        if (searchInput) {
            keyword = searchInput.value.trim();
        }
        
        if (sortSelect) {
            sort = sortSelect.value;
        }
        
        // 更新分页激活状态
        const paginationContainer = document.querySelector('.pagination-container');
        if (paginationContainer) {
            // 移除当前活动页
            const activeBtn = paginationContainer.querySelector('.page-btn.active');
            if (activeBtn) {
                activeBtn.classList.remove('active');
            }
            
            // 添加新的活动页
            const newActiveBtn = paginationContainer.querySelector(`.page-btn[data-page="${pageNum}"]`);
            if (newActiveBtn) {
                newActiveBtn.classList.add('active');
            }
            
            // 更新上一页/下一页按钮状态
            const prevBtn = paginationContainer.querySelector('.prev-btn');
            const nextBtn = paginationContainer.querySelector('.next-btn');
            
            if (prevBtn) {
                prevBtn.disabled = pageNum <= 1;
            }
            
            if (nextBtn) {
                const totalPages = Math.ceil(parseInt(paginationContainer.dataset.total || '0') / 6);
                nextBtn.disabled = pageNum >= totalPages;
            }
        }
        
        // 平滑滚动到内容区顶部
        const userContent = document.querySelector('.user-content');
        if (userContent) {
            userContent.scrollIntoView({ behavior: 'smooth', block: 'start' });
        }
        
        // 获取用户博客并更新页面
        fetchUserBlogs(keyword, sort, pageNum, 6);
        
        // 更新URL参数
        const url = new URL(window.location.href);
        url.searchParams.set('page', pageNum);
        if (keyword) url.searchParams.set('keyword', keyword);
        if (sort !== 'postTime') url.searchParams.set('sort', sort);
        window.history.pushState({}, '', url);
    }
}

// 显示通知
function showNotification(message, type = 'info') {
    // 检查是否已有通知
    let notification = document.querySelector('.notification');
    if (!notification) {
        notification = document.createElement('div');
        notification.className = 'notification';
        document.body.appendChild(notification);
    }
    
    // 设置通知样式和内容
    notification.className = `notification ${type}`;
    
    // 设置图标
    let icon = '';
    switch (type) {
        case 'success':
            icon = '<i class="fas fa-check-circle"></i>';
            break;
        case 'error':
            icon = '<i class="fas fa-exclamation-circle"></i>';
            break;
        case 'info':
        default:
            icon = '<i class="fas fa-info-circle"></i>';
    }
    
    notification.innerHTML = `
        ${icon}
        <span>${message}</span>
        <button class="close-btn"><i class="fas fa-times"></i></button>
    `;
    
    // 显示通知
    setTimeout(() => {
        notification.classList.add('show');
    }, 10);
    
    // 绑定关闭按钮
    const closeBtn = notification.querySelector('.close-btn');
    closeBtn.addEventListener('click', () => {
        notification.classList.remove('show');
        setTimeout(() => {
            notification.remove();
        }, 300);
    });
    
    // 自动关闭
    setTimeout(() => {
        if (notification.parentNode) {
            notification.classList.remove('show');
            setTimeout(() => {
                if (notification.parentNode) {
                    notification.remove();
                }
            }, 300);
        }
    }, 5000);
}

// 绑定修改密码按钮点击事件
function bindPasswordChangeBtn() {
    const changePasswordBtn = document.getElementById('changePasswordBtn');
    if (changePasswordBtn) {
        changePasswordBtn.addEventListener('click', function() {
            showPasswordChangeModal();
        });
    }
}

// 显示修改密码模态框 - 完善版
function showPasswordChangeModal() {
    // 创建模态框HTML
    const modalHtml = `
        <div class="modal-overlay" id="passwordModal">
            <div class="modal-container">
                <div class="modal-header">
                    <h3>修改密码</h3>
                    <button class="modal-close-btn" id="modalCloseBtn"><i class="fas fa-times"></i></button>
                </div>
                <div class="modal-body">
                    <form id="passwordChangeForm">
                        <div class="form-group">
                            <label for="oldPassword">原密码</label>
                            <div class="password-input-wrapper">
                                <input type="password" id="oldPassword" placeholder="请输入原密码" required>
                                <button type="button" class="toggle-password">
                                    <i class="fas fa-eye"></i>
                                </button>
                            </div>
                        </div>
                        
                        <div class="form-group">
                            <label for="newPassword">新密码</label>
                            <div class="password-input-wrapper">
                                <input type="password" id="newPassword" placeholder="请输入新密码" required>
                                <button type="button" class="toggle-password">
                                    <i class="fas fa-eye"></i>
                                </button>
                            </div>
                            <div class="password-strength-meter">
                                <div class="meter-bar">
                                    <div class="meter-fill" style="width: 0%"></div>
                                </div>
                                <div class="strength-text">密码强度: 弱</div>
                            </div>
                            <p class="password-tips">密码至少包含6个字符，建议使用字母、数字和特殊字符的组合</p>
                        </div>
                        
                        <div class="form-group">
                            <label for="confirmPassword">确认新密码</label>
                            <div class="password-input-wrapper">
                                <input type="password" id="confirmPassword" placeholder="请再次输入新密码" required>
                                <button type="button" class="toggle-password">
                                    <i class="fas fa-eye"></i>
                                </button>
                            </div>
                            <p class="match-text"></p>
                        </div>
                        
                        <div class="btn-group">
                            <button type="button" class="btn btn-secondary" id="cancelBtn">取消</button>
                            <button type="submit" class="btn btn-primary" id="submitBtn">保存</button>
                        </div>
                    </form>
                </div>
            </div>
        </div>
    `;
    
    // 添加模态框到页面
    document.body.insertAdjacentHTML('beforeend', modalHtml);
    
    // 绑定模态框事件
    const modal = document.getElementById('passwordModal');
    const closeBtn = document.getElementById('modalCloseBtn');
    const cancelBtn = document.getElementById('cancelBtn');
    const form = document.getElementById('passwordChangeForm');
    const newPasswordInput = document.getElementById('newPassword');
    const confirmPasswordInput = document.getElementById('confirmPassword');
    const submitBtn = document.getElementById('submitBtn');
    const matchText = document.querySelector('.match-text');
    
    // 关闭模态框函数
    const closeModal = () => {
        modal.classList.add('closing');
        setTimeout(() => {
            modal.remove();
        }, 300);
    };
    
    // 绑定密码显示/隐藏切换
    const toggleButtons = document.querySelectorAll('.toggle-password');
    toggleButtons.forEach(button => {
        button.addEventListener('click', function() {
            const input = this.previousElementSibling;
            const icon = this.querySelector('i');
            
            if (input.type === 'password') {
                input.type = 'text';
                icon.classList.remove('fa-eye');
                icon.classList.add('fa-eye-slash');
            } else {
                input.type = 'password';
                icon.classList.remove('fa-eye-slash');
                icon.classList.add('fa-eye');
            }
        });
    });
    
    // 密码强度检测
    newPasswordInput.addEventListener('input', function() {
        const password = this.value;
        const strengthMeter = document.querySelector('.meter-fill');
        const strengthText = document.querySelector('.strength-text');
        
        // 检查密码强度
        let strength = 0;
        let strengthLabel = '弱';
        let color = '#e74c3c';
        
        if (password.length >= 6) {
            strength += 20;
        }
        
        if (password.length >= 8) {
            strength += 10;
        }
        
        if (/[A-Z]/.test(password)) {
            strength += 20;
        }
        
        if (/[0-9]/.test(password)) {
            strength += 20;
        }
        
        if (/[^A-Za-z0-9]/.test(password)) {
            strength += 30;
        }
        
        // 更新强度显示
        if (strength >= 80) {
            strengthLabel = '很强';
            color = '#27ae60';
        } else if (strength >= 50) {
            strengthLabel = '中等';
            color = '#f39c12';
        } else if (strength >= 30) {
            strengthLabel = '弱';
            color = '#e74c3c';
        } else {
            strengthLabel = '非常弱';
            color = '#c0392b';
        }
        
        strengthMeter.style.width = strength + '%';
        strengthMeter.style.backgroundColor = color;
        strengthText.textContent = '密码强度: ' + strengthLabel;
        
        // 检查确认密码是否匹配
        checkPasswordMatch();
    });
    
    // 检查密码匹配
    function checkPasswordMatch() {
        const newPassword = newPasswordInput.value;
        const confirmPassword = confirmPasswordInput.value;
        
        if (confirmPassword.length > 0) {
            if (newPassword === confirmPassword) {
                matchText.textContent = '密码匹配';
                matchText.style.color = '#27ae60';
                confirmPasswordInput.style.borderColor = '#27ae60';
            } else {
                matchText.textContent = '密码不匹配';
                matchText.style.color = '#e74c3c';
                confirmPasswordInput.style.borderColor = '#e74c3c';
            }
        } else {
            matchText.textContent = '';
            confirmPasswordInput.style.borderColor = '';
        }
    }
    
    // 监听确认密码输入
    confirmPasswordInput.addEventListener('input', checkPasswordMatch);
    
    // 绑定关闭按钮
    closeBtn.addEventListener('click', closeModal);
    cancelBtn.addEventListener('click', closeModal);
    
    // 点击模态框外部关闭
    modal.addEventListener('click', function(e) {
        if (e.target === modal) {
            closeModal();
        }
    });
    
    // 表单提交处理
    form.addEventListener('submit', function(e) {
        e.preventDefault();
        
        const oldPassword = document.getElementById('oldPassword').value;
        const newPassword = newPasswordInput.value;
        const confirmPassword = confirmPasswordInput.value;
        
        // 禁用提交按钮，防止重复提交
        submitBtn.disabled = true;
        submitBtn.innerHTML = '<i class="fas fa-spinner fa-spin"></i> 处理中...';
        
        // 验证新密码长度
        if (newPassword.length < 6) {
            showNotification('新密码长度至少为6个字符', 'error');
            submitBtn.disabled = false;
            submitBtn.textContent = '保存';
            return;
        }
        
        // 验证新密码和确认密码是否一致
        if (newPassword !== confirmPassword) {
            showNotification('新密码和确认密码不一致', 'error');
            submitBtn.disabled = false;
            submitBtn.textContent = '保存';
            return;
        }
        
        // 发送请求修改密码
        changePassword(oldPassword, newPassword, closeModal, () => {
            submitBtn.disabled = false;
            submitBtn.textContent = '保存';
        });
    });
    
    // 显示模态框动画
    setTimeout(() => {
        modal.classList.add('show');
    }, 10);
}

// 修改密码函数 - 完善版
function changePassword(oldPassword, newPassword, successCallback, errorCallback) {
    // 显示加载中通知
    showNotification('修改密码中...', 'info');
    
    // 构造JSON请求数据
    const requestData = {
        oldPassword: oldPassword,
        newPassword: newPassword
    };

    // 发送请求
    fetch('/api/user/update_password', {
        method: 'PATCH',
        headers: {
            'Content-Type': 'application/json',
            'Accept': 'application/json'
        },
        body: JSON.stringify(requestData)
    })
    .then(response => {
        if (!response.ok) {
            if (response.status === 401 || response.status === 403) {
                throw new Error('原密码不正确');
            } else if (response.status === 400) {
                return response.json().then(errData => {
                    throw new Error(errData.message || '请求参数错误');
                });
            } else {
                throw new Error('修改密码失败');
            }
        }
        return response.json();
    })
    .then(data => {
        showNotification(data.message || '密码修改成功！', 'success');
        if (successCallback) successCallback();
    })
    .catch(error => {
        console.error('修改密码失败:', error);
        showNotification(error.message || '密码修改失败，请重试', 'error');
        if (errorCallback) errorCallback();
    });
}

// CSS添加到页面主体
const styleElement = document.createElement('style');
styleElement.textContent = `
/* 用户中心容器 */
.user-center-container {
    display: flex;
    margin-top: 20px;
    gap: 20px;
    max-width: 1200px;
    margin-left: auto;
    margin-right: auto;
}

/* 侧边栏样式 */
.user-sidebar {
    width: 250px;
    flex-shrink: 0;
    background-color: var(--card-bg);
    border-radius: 10px;
    overflow: hidden;
    box-shadow: 0 2px 10px rgba(0, 0, 0, 0.05);
}

.profile-preview {
    padding: 20px;
    text-align: center;
    background-color: var(--sidebar-header-bg);
}

.user-avatar {
    width: 100px;
    height: 100px;
    border-radius: 50%;
    object-fit: cover;
    margin-bottom: 10px;
    border: 4px solid var(--card-bg);
    cursor: pointer;
    transition: transform 0.3s ease;
}

.avatar-wrapper {
    position: relative;
    display: inline-block;
}

.hover-tip {
    position: absolute;
    top: 50%;
    left: 50%;
    transform: translate(-50%, -50%);
    background-color: rgba(0, 0, 0, 0.7);
    color: white;
    padding: 5px 10px;
    border-radius: 4px;
    font-size: 12px;
    opacity: 0;
    transition: opacity 0.3s;
    pointer-events: none;
}

.avatar-wrapper:hover .user-avatar {
    transform: scale(1.05);
}

.avatar-wrapper:hover .hover-tip {
    opacity: 1;
}

.profile-preview h3 {
    margin: 0;
    color: var(--text-color);
    font-size: 1.2rem;
}

.user-nav-links {
    list-style: none;
    padding: 0;
    margin: 0;
}

.user-nav-links li {
    padding: 15px 20px;
    cursor: pointer;
    transition: background-color 0.3s;
    color: var(--text-color);
    display: flex;
    align-items: center;
    gap: 10px;
    border-bottom: 1px solid var(--border-color-light);
}

.user-nav-links li:hover {
    background-color: var(--hover-color);
}

.user-nav-links li.active {
    background-color: var(--primary-color);
    color: white;
}

.user-nav-links li i {
    width: 20px;
    text-align: center;
}

/* 内容区域样式 */
.user-content {
    flex: 1;
    background-color: var(--card-bg);
    border-radius: 10px;
    padding: 30px;
    box-shadow: 0 2px 10px rgba(0, 0, 0, 0.05);
}

.user-content h2 {
    margin-top: 0;
    margin-bottom: 20px;
    color: var(--text-color);
    font-size: 1.5rem;
    border-bottom: 1px solid var(--border-color);
    padding-bottom: 10px;
}

/* 加载动画 */
.loading {
    text-align: center;
    padding: 40px 0;
    color: var(--secondary-text-color);
}

.loading i {
    font-size: 2rem;
    margin-bottom: 15px;
    display: block;
}

/* 个人资料页样式 */
.profile-info {
    color: var(--text-color);
}

.profile-section {
    margin-bottom: 30px;
}

.profile-section h3 {
    font-size: 1.2rem;
    margin-bottom: 15px;
    color: var(--text-color);
}

.info-item {
    display: flex;
    margin-bottom: 15px;
    align-items: center;
}

.info-item label {
    width: 100px;
    font-weight: bold;
    color: var(--secondary-text-color);
}

.info-item p {
    margin: 0;
    line-height: 1.5;
}

.profile-stats {
    display: flex;
    flex-wrap: wrap;
    gap: 15px;
}

.stat-item {
    flex: 1;
    min-width: 120px;
    background-color: var(--stat-bg);
    border-radius: 8px;
    padding: 15px;
    text-align: center;
}

.stat-value {
    font-size: 1.5rem;
    font-weight: bold;
    color: var(--primary-color);
    margin-bottom: 5px;
}

.stat-label {
    color: var(--secondary-text-color);
    font-size: 0.9rem;
}

/* 搜索和排序 */
.manage-filter {
    display: flex;
    justify-content: space-between;
    margin-bottom: 20px;
    gap: 15px;
}

.search-input, .sort-select {
    padding: 10px 15px;
    border: 1px solid var(--border-color);
    border-radius: 5px;
    background-color: var(--input-bg);
    color: var(--text-color);
}

.search-input {
    flex: 1;
}

.sort-select {
    width: 150px;
}

/* 博客卡片列表 */
.manage-list {
    display: grid;
    grid-template-columns: repeat(auto-fill, minmax(280px, 1fr));
    gap: 20px;
}

.manage-card {
`;

// 添加模态框样式
styleElement.textContent += `
/* 模态框样式 */
.modal-overlay {
    position: fixed;
    top: 0;
    left: 0;
    right: 0;
    bottom: 0;
    background-color: rgba(0, 0, 0, 0.5);
    display: flex;
    align-items: center;
    justify-content: center;
    z-index: 1000;
    opacity: 0;
    transition: opacity 0.3s ease;
}

.modal-overlay.show {
    opacity: 1;
}

.modal-overlay.closing {
    opacity: 0;
}

.modal-container {
    background-color: var(--card-bg);
    border-radius: 10px;
    width: 100%;
    max-width: 500px;
    box-shadow: 0 5px 15px rgba(0, 0, 0, 0.2);
    transform: translateY(-20px);
    transition: transform 0.3s ease;
}

.modal-overlay.show .modal-container {
    transform: translateY(0);
}

.modal-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 15px 20px;
    border-bottom: 1px solid var(--border-color);
}

.modal-header h3 {
    margin: 0;
    color: var(--text-color);
}

.modal-close-btn {
    background: none;
    border: none;
    font-size: 1.2rem;
    cursor: pointer;
    color: var(--secondary-text-color);
}

.modal-body {
    padding: 20px;
}

#passwordChangeForm .form-group {
    margin-bottom: 20px;
}

#passwordChangeForm label {
    display: block;
    margin-bottom: 8px;
    font-weight: bold;
    color: var(--text-color);
}

#passwordChangeForm input {
    width: 100%;
    padding: 12px 15px;
    border: 1px solid var(--border-color);
    border-radius: 5px;
    background-color: var(--input-bg);
    color: var(--text-color);
}

#passwordChangeForm .btn-group {
    display: flex;
    justify-content: flex-end;
    gap: 10px;
    margin-top: 30px;
}

/* 密码输入框样式 */
.password-input-wrapper {
    position: relative;
    display: flex;
    align-items: center;
}

.password-input-wrapper input {
    width: 100%;
    padding-right: 40px;
}

.toggle-password {
    position: absolute;
    right: 10px;
    background: none;
    border: none;
    color: var(--secondary-text-color);
    cursor: pointer;
    padding: 0;
    font-size: 1rem;
}

.toggle-password:hover {
    color: var(--primary-color);
}

/* 密码强度指示器 */
.password-strength-meter {
    margin-top: 10px;
}

.meter-bar {
    height: 5px;
    background-color: var(--border-color);
    border-radius: 2px;
    margin-bottom: 5px;
}

.meter-fill {
    height: 100%;
    width: 0;
    border-radius: 2px;
    transition: width 0.3s ease, background-color 0.3s ease;
}

.strength-text {
    font-size: 0.8rem;
    color: var(--secondary-text-color);
}

.password-tips {
    margin-top: 5px;
    font-size: 0.8rem;
    color: var(--secondary-text-color);
}

.match-text {
    margin-top: 5px;
    font-size: 0.8rem;
    height: 1rem;
}

/* 模态框动画改进 */
.modal-overlay {
    backdrop-filter: blur(2px);
}

.modal-container {
    max-width: 450px;
    width: 90%;
}

.modal-overlay.show .modal-container {
    animation: modalSlideIn 0.3s ease forwards;
}

@keyframes modalSlideIn {
    from {
        transform: scale(0.9) translateY(-30px);
        opacity: 0;
    }
    to {
        transform: scale(1) translateY(0);
        opacity: 1;
    }
}

.modal-overlay.closing .modal-container {
    animation: modalSlideOut 0.3s ease forwards;
}

@keyframes modalSlideOut {
    from {
        transform: scale(1) translateY(0);
        opacity: 1;
    }
    to {
        transform: scale(0.9) translateY(30px);
        opacity: 0;
    }
}
`;

document.head.appendChild(styleElement);

// 添加防抖函数
function debounce(func, wait) {
    let timeout;
    return function() {
        const context = this;
        const args = arguments;
        clearTimeout(timeout);
        timeout = setTimeout(() => {
            func.apply(context, args);
        }, wait);
    };
}

// 加载分类列表
async function loadCategories(selectedCategory) {
    try {
        const response = await fetch('/api/categories');
        if (!response.ok) {
            throw new Error('获取分类列表失败');
        }
        
        const result = await response.json();
        const categorySelect = document.getElementById('blogCategory');
        categorySelect.innerHTML = '<option value="">选择分类</option>';
        
        // 添加默认分类
        const defaultCategories = [
            { id: 'tech', name: '技术' },
            { id: 'life', name: '生活' },
            { id: 'thoughts', name: '随想' },
            { id: 'other', name: '其他' }
        ];
        
        defaultCategories.forEach(category => {
            const option = document.createElement('option');
            option.value = category.id;
            option.textContent = category.name;
            if (selectedCategory === category.id) {
                option.selected = true;
            }
            categorySelect.appendChild(option);
        });
        
        // 如果API返回了分类列表，则使用API返回的数据
        if (result.data && result.data.categories) {
            result.data.categories.forEach(category => {
                const option = document.createElement('option');
                option.value = category.id;
                option.textContent = category.name;
                if (selectedCategory === category.id) {
                    option.selected = true;
                }
                categorySelect.appendChild(option);
            });
        }
    } catch (error) {
        console.error('加载分类失败:', error);
        showNotification('加载分类失败，使用默认分类', 'warning');
    }
}

// 编辑博客
async function editBlog(blogId) {
    if (!blogId) {
        showNotification('无效的博客ID', 'error');
        return;
    }
    
    try {
        // 显示加载状态
        showNotification('正在加载博客数据...', 'info');
        
        // 获取博客详情
        const response = await fetch(`/api/blogs/${blogId}`);
        
        if (!response.ok) {
            throw new Error('获取博客数据失败');
        }
        
        let blogData = await response.json();
        
        // 处理不同格式的API响应
        if (blogData.code && blogData.data) {
            blogData = blogData.data;
        }
        
        // 确保统一封面图片字段名
        if (!blogData.coverImage) {
            // 尝试其他可能的字段名
            blogData.coverImage = blogData.cover_image || blogData.thumbnail || blogData.cover || '';
            console.log('统一封面图片字段:', blogData.coverImage);
        }
        
        // 确保标签字段格式正确
        if (typeof blogData.tags === 'string') {
            try {
                // 尝试解析JSON字符串
                const parsedTags = JSON.parse(blogData.tags);
                if (Array.isArray(parsedTags)) {
                    blogData.tags = parsedTags;
                } else {
                    // 如果解析结果不是数组，假设是逗号分隔的字符串
                    blogData.tags = blogData.tags.split(',').map(tag => tag.trim()).filter(tag => tag);
                }
            } catch (e) {
                // 如果解析失败，假设是逗号分隔的字符串
                blogData.tags = blogData.tags.split(',').map(tag => tag.trim()).filter(tag => tag);
            }
        } else if (!Array.isArray(blogData.tags)) {
            // 如果不是字符串也不是数组，设为空数组
            blogData.tags = [];
        }
        
        // 将博客数据存储到sessionStorage
        sessionStorage.setItem('editBlogData', JSON.stringify(blogData));
        
        // 跳转到博客编辑页面
        window.location.href = `blog_editor.html?mode=edit&id=${blogId}`;
    } catch (error) {
        console.error('加载博客数据失败:', error);
        showNotification('加载博客数据失败，请重试', 'error');
    }
}

// 更新设置表单内容 - 新增函数
function updateSettingsForm(userData) {
    if (!userData) return;
    
    const usernameInput = document.getElementById('username');
    const emailInput = document.getElementById('email');
    const bioInput = document.getElementById('bio');
    
    // 填充表单字段
    if (usernameInput) usernameInput.value = userData.username || '';
    if (emailInput) emailInput.value = userData.email || '';
    if (bioInput) bioInput.value = userData.bio || '';
}