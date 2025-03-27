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

// 生成博客管理HTML
function generateManageHTML() {
    // 模拟博客数据
    const blogs = [
        { id: 1, title: 'JavaScript高级编程技巧，带你入门函数式编程', date: '2023-10-15', views: 256, likes: 45, image: 'img/blog-1.jpg' },
        { id: 2, title: 'CSS Grid布局完全指南', date: '2023-09-20', views: 189, likes: 32, image: 'img/blog-2.jpg' },
        { id: 3, title: 'Vue3和React18的异同点分析', date: '2023-08-10', views: 352, likes: 68, image: 'img/blog-3.jpg' },
        { id: 4, title: '5分钟搭建一个Node.js服务器', date: '2023-07-05', views: 123, likes: 19, image: 'img/blog-4.jpg' },
        { id: 5, title: '前端开发中常见的安全问题及解决方案', date: '2023-06-18', views: 210, likes: 41, image: 'img/blog-5.jpg' },
    ];
    
    let html = `
        <h2>博客管理</h2>
        <div class="manage-filter">
            <input type="text" placeholder="搜索博客标题" class="search-input">
            <select class="sort-select">
                <option value="date">按日期排序</option>
                <option value="views">按浏览量排序</option>
                <option value="likes">按点赞数排序</option>
            </select>
        </div>
        <div class="manage-list">
    `;
    
    blogs.forEach(blog => {
        html += `
            <div class="manage-card" data-id="${blog.id}">
                <div class="blog-thumbnail">
                    <img src="${blog.image}" alt="${blog.title}">
                </div>
                <div class="blog-info">
                    <div class="blog-title">${blog.title}</div>
                    <div class="blog-meta">
                        <span><i class="far fa-calendar-alt"></i> ${blog.date}</span>
                        <span><i class="far fa-eye"></i> ${blog.views}</span>
                        <span><i class="far fa-thumbs-up"></i> ${blog.likes}</span>
                    </div>
                    <div class="blog-actions">
                        <button class="edit-btn"><i class="far fa-edit"></i> 编辑</button>
                        <button class="delete-btn"><i class="far fa-trash-alt"></i> 删除</button>
                    </div>
                </div>
            </div>
        `;
    });
    
    html += `
        </div>
    `;
    
    return html;
}

// 生成设置页面HTML
function generateSettingsHTML() {
    return `
        <h2>账号设置</h2>
        <div class="settings-form">
            <form id="settingsForm">
                <div class="form-group">
                    <label for="username">用户名</label>
                    <input type="text" id="username" value="云中杉木" placeholder="请输入用户名">
                </div>
                
                <div class="form-group">
                    <label for="email">邮箱</label>
                    <input type="email" id="email" value="example@email.com" placeholder="请输入邮箱">
                </div>
                
                <div class="form-group">
                    <label for="bio">个人简介</label>
                    <textarea id="bio" rows="5" placeholder="请输入个人简介">热爱编程和技术分享的开发者，喜欢探索新技术，分享学习心得。</textarea>
                </div>
                
                <div class="form-group">
                    <label>主题设置</label>
                    <div class="theme-options">
                        <label>
                            <input type="radio" name="theme" value="light" checked> 浅色主题
                        </label>
                        <label>
                            <input type="radio" name="theme" value="dark"> 深色主题
                        </label>
                        <label>
                            <input type="radio" name="theme" value="auto"> 跟随系统
                        </label>
                    </div>
                </div>
                
                <div class="btn-group">
                    <button type="button" class="btn btn-secondary" id="resetBtn">重置</button>
                    <button type="submit" class="btn btn-primary">保存更改</button>
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
        if (body.classList.contains('dark-theme')) {
            body.classList.remove('dark-theme');
            themeToggle.innerHTML = '<i class="fas fa-moon"></i>';
            localStorage.setItem('theme', 'light');
        } else {
            body.classList.add('dark-theme');
            themeToggle.innerHTML = '<i class="fas fa-sun"></i>';
            localStorage.setItem('theme', 'dark');
        }
    });
}

// 检查主题设置
function checkThemeSetting() {
    const savedTheme = localStorage.getItem('theme');
    const themeToggle = document.getElementById('themeToggle');
    
    if (savedTheme === 'dark') {
        document.body.classList.add('dark-theme');
        themeToggle.innerHTML = '<i class="fas fa-sun"></i>';
    } else if (savedTheme === null) {
        // 检查系统主题偏好
        if (window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches) {
            document.body.classList.add('dark-theme');
            themeToggle.innerHTML = '<i class="fas fa-sun"></i>';
        }
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
    // 编辑按钮事件
    const editButtons = document.querySelectorAll('.edit-btn');
    editButtons.forEach(btn => {
        btn.addEventListener('click', function() {
            const blogId = this.closest('.manage-card').getAttribute('data-id');
            // 跳转到编辑页面
            window.location.href = `blog_editor.html?id=${blogId}`;
        });
    });
    
    // 删除按钮事件
    const deleteButtons = document.querySelectorAll('.delete-btn');
    deleteButtons.forEach(btn => {
        btn.addEventListener('click', function() {
            const blogId = this.closest('.manage-card').getAttribute('data-id');
            if (confirm('确定要删除这篇博客吗？此操作不可恢复！')) {
                // 发送删除请求
                deleteBlog(blogId);
            }
        });
    });
    
    // 搜索功能
    const searchInput = document.querySelector('.search-input');
    searchInput.addEventListener('input', function() {
        const keyword = this.value.toLowerCase();
        const blogCards = document.querySelectorAll('.manage-card');
        
        blogCards.forEach(card => {
            const title = card.querySelector('.blog-title').textContent.toLowerCase();
            if (title.includes(keyword)) {
                card.style.display = 'block';
            } else {
                card.style.display = 'none';
            }
        });
    });
    
    // 排序功能
    const sortSelect = document.querySelector('.sort-select');
    sortSelect.addEventListener('change', function() {
        const sortType = this.value;
        const blogList = document.querySelector('.manage-list');
        const blogCards = Array.from(document.querySelectorAll('.manage-card'));
        
        blogCards.sort((a, b) => {
            if (sortType === 'date') {
                const dateA = a.querySelector('.blog-meta span:first-child').textContent;
                const dateB = b.querySelector('.blog-meta span:first-child').textContent;
                return new Date(dateB) - new Date(dateA);
            } else if (sortType === 'views') {
                const viewsA = parseInt(a.querySelector('.blog-meta span:nth-child(2)').textContent.match(/\d+/)[0]);
                const viewsB = parseInt(b.querySelector('.blog-meta span:nth-child(2)').textContent.match(/\d+/)[0]);
                return viewsB - viewsA;
            } else if (sortType === 'likes') {
                const likesA = parseInt(a.querySelector('.blog-meta span:nth-child(3)').textContent.match(/\d+/)[0]);
                const likesB = parseInt(b.querySelector('.blog-meta span:nth-child(3)').textContent.match(/\d+/)[0]);
                return likesB - likesA;
            }
        });
        
        // 重新添加排序后的卡片
        blogCards.forEach(card => {
            blogList.appendChild(card);
        });
    });
}

// 绑定设置表单事件
function bindSettingsEvents() {
    const settingsForm = document.getElementById('settingsForm');
    const resetBtn = document.getElementById('resetBtn');
    
    // 保存设置
    settingsForm.addEventListener('submit', function(e) {
        e.preventDefault();
        
        const formData = {
            username: document.getElementById('username').value,
            email: document.getElementById('email').value,
            bio: document.getElementById('bio').value,
            theme: document.querySelector('input[name="theme"]:checked').value
        };
        
        // 发送更新请求
        updateUserSettings(formData);
    });
    
    // 重置表单
    resetBtn.addEventListener('click', function() {
        settingsForm.reset();
    });
    
    // 主题切换
    const themeOptions = document.querySelectorAll('input[name="theme"]');
    themeOptions.forEach(option => {
        option.addEventListener('change', function() {
            const themeValue = this.value;
            const body = document.body;
            const themeToggle = document.getElementById('themeToggle');
            
            if (themeValue === 'light') {
                body.classList.remove('dark-theme');
                themeToggle.innerHTML = '<i class="fas fa-moon"></i>';
                localStorage.setItem('theme', 'light');
            } else if (themeValue === 'dark') {
                body.classList.add('dark-theme');
                themeToggle.innerHTML = '<i class="fas fa-sun"></i>';
                localStorage.setItem('theme', 'dark');
            } else if (themeValue === 'auto') {
                // 检查系统偏好
                if (window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches) {
                    body.classList.add('dark-theme');
                    themeToggle.innerHTML = '<i class="fas fa-sun"></i>';
                } else {
                    body.classList.remove('dark-theme');
                    themeToggle.innerHTML = '<i class="fas fa-moon"></i>';
                }
                localStorage.removeItem('theme');
            }
        });
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

// 更新用户设置
function updateUserSettings(formData) {
    // 显示保存中提示
    showNotification('保存中...', 'info');
    
    // 发送请求
    fetch('/api/user/settings', {
        method: 'POST',
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
    })
    .catch(error => {
        showNotification('保存失败：' + error.message, 'error');
    });
}

// 删除博客
function deleteBlog(blogId) {
    // 显示删除中提示
    showNotification('正在删除...', 'info');
    
    // 发送请求
    fetch(`/api/blogs/${blogId}`, {
        method: 'DELETE'
    })
    .then(response => {
        if (!response.ok) {
            throw new Error('删除失败');
        }
        return response.json();
    })
    .then(data => {
        // 删除成功，移除对应博客卡片
        const blogCard = document.querySelector(`.manage-card[data-id="${blogId}"]`);
        if (blogCard) {
            blogCard.remove();
        }
        
        showNotification('博客已删除', 'success');
        
        // 检查是否还有博客
        const remainingCards = document.querySelectorAll('.manage-card');
        if (remainingCards.length === 0) {
            document.querySelector('.manage-list').innerHTML = '<p>暂无博客内容</p>';
        }
    })
    .catch(error => {
        showNotification('删除失败：' + error.message, 'error');
    });
}

// 标记消息为已读
function markMessageAsRead(messageId) {
    fetch(`/api/messages/${messageId}/read`, {
        method: 'POST'
    })
    .then(response => {
        if (!response.ok) {
            throw new Error('操作失败');
        }
        return response.json();
    })
    .then(data => {
        // 不需要显示提示
    })
    .catch(error => {
        console.error('标记消息已读失败：', error);
    });
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