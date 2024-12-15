document.addEventListener('DOMContentLoaded', function() {
    // 获取所有导航项和页面
    const navItems = document.querySelectorAll('.nav-links li');
    const pages = document.querySelectorAll('.page');

    // 为每个导航项添加点击事件
    navItems.forEach(item => {
        item.addEventListener('click', function() {
            // 移除所有导航项的active类
            navItems.forEach(nav => nav.classList.remove('active'));
            // 为当前点击的导航项添加active类
            this.classList.add('active');

            // 获取对应的页面ID
            const pageId = this.getAttribute('data-page');
            
            // 隐藏所有页面
            pages.forEach(page => page.classList.remove('active'));
            // 显示当前选中的页面
            document.getElementById(pageId).classList.add('active');
        });
    });
    loadPageData('profile'); // 默认加载个人资料页面
    // loadPageData('manage');
});

// 获取用户的头像和用户名
function getCurrentUsername() {
    $.ajax({
        type: 'get',
        url: 'get_current_user', // 这里的URL是你的服务器端API，用于获取当前用户
        success: function(response) {
            if (response.username) {
                document.getElementById('avatar').src = response.avatar; // 替换用户头像
                document.getElementById('username').innerHTML = response.username; // 替换用户名
                // document.getElementById('article_count').innerHTML = response.article_count;
            }
        },
        error: function(xhr, status, error) {
            console.error('获取用户名失败:', status, error);
        }
    });
}
// 调用获取用户名的函数
getCurrentUsername();

// 根据传入的选择，加载不同的页面
function loadPageData(page) {
    fetch(`/${page}`, {
        method: 'GET',
        headers: {
            'Content-Type': 'application/json',
        },
    })
        .then(response => response.json())
        .then(data => updatePageContent(page, data))
        .catch(error => console.error('加载数据失败:', error));
}

// 根据选择切换不同的页面
function updatePageContent(page, data) {
    const content = document.getElementById('content');
    content.innerHTML = ''; // 清空旧内容

    // 个人资料
    if (page === 'profile') {
        content.innerHTML = `
            <h2>个人资料</h2>
            <div class="profile-info">
                <div class="info-item"><label>昵称：</label><span>${data.username}</span></div>
                <div class="info-item"><label>邮箱：</label><span>${data.email}</span></div>
                <div class="info-item"><label>简介：</label><p>${data.description}</p></div>
            </div>`;
    }
    //博客管理
    else if (page === 'manage') {
        if (Array.isArray(data.blogs) && data.blogs.length > 0) {
            content.innerHTML = `<h2>博客管理</h2>
            <div class="manage-list">
                ${data.blogs.map(blog => `
                <div class="manage-card">
                    <h3 class="manage-title">${blog.title}</h3>
                    <p class="manage-time">发布时间：${blog.postTime}</p>
                    <div class="manage-actions">
                        <button class="edit-btn" onclick="editBlog(${blog.blogId})">编辑</button>
                        <button class="delete-btn" onclick="deleteBlog(${blog.blogId})">删除</button>
                    </div>
                </div>
                `).join('')}
            </div>
            `;
        } else {
            content.innerHTML = `<h2>博客管理</h2><p>暂无博客内容。</p>`;
        }
    }
    // 账号设置
    else if (page === 'settings') {
        content.innerHTML = `
            <h2>账号设置</h2>
            <div class="settings-form">
                <div class="form-item"><label>原密码</label><input type="password" id="old-password" placeholder="请输入原密码"></div>
                <div class="form-item"><label>修改密码</label><input type="password" id="new-password" placeholder="请输入新密码"></div>
                <div class="form-item"><label>确认密码</label><input type="password" id="confirm-password" placeholder="请确认新密码"></div>
                <button class="save-btn" onclick="savePasswordChange()">保存修改</button>
            </div>`;
    }

    // 消息中心
    else if (page === 'messages') {
        // content.innerHTML = `<h2>消息中心</h2><div class="message-list">${
        //     mockMessages.map(msg => `
        //         <div class="message-item">
        //             <h4>${msg.type}</h4>
        //             <p>${msg.content}</p>
        //             <span class="time">${msg.time}</span>
        //         </div>
        //     `).join('')
        // }</div>`;
        MessageModel.fetchMessages();
        MessageModel.updateMessageDisplay();
    }
}

// 博客管理-编辑博客
function editBlog(blogId) {
    // 跳转到博客编辑页面，传递 blogId 参数
    window.location.href = `blog_editor_modify.html?blogId=${blogId}`;
}

// 博客管理-删除博客
function deleteBlog(blogId) {
    // 创建模态框
    const modalHtml = `
        <div id="confirmModal" class="modal">
            <div class="modal-content">
                <h3>确认删除</h3>
                <p>确定要删除这篇博客吗？此操作无法撤销。</p>
                <div class="modal-actions">
                    <button class="btn-confirm" onclick="confirmDelete(${blogId})">确认</button>
                    <button class="btn-cancel" onclick="closeModal()">取消</button>
                </div>
            </div>
        </div>
    `;
    document.body.insertAdjacentHTML('beforeend', modalHtml);
}
// 博客管理-确认删除按钮
function confirmDelete(blogId) {
    fetch(`/delete_blog?blogId=${blogId}`, {
        method: 'DELETE',
        headers: { 'Content-Type': 'application/json' },
    })
    .then(response => {
        if (response.ok) {
            alert('删除成功');
            closeModal();
            loadPageData('manage'); // 重新加载博客管理页面
        } else {
            alert('删除失败');
        }
    })
    .catch(error => console.error('删除博客失败:', error));
}
function closeModal() {
    const modal = document.getElementById('confirmModal');
    if (modal) modal.remove();
}


// 账号设置-修改密码
function savePasswordChange() {
    const oldPassword = document.getElementById('old-password').value;
    const newPassword = document.getElementById('new-password').value;
    const confirmPassword = document.getElementById('confirm-password').value;

    // 校验密码一致性
    if (newPassword !== confirmPassword) {
        alert('新密码和确认密码不一致！');
        return;
    }

    // 准备表单数据
    const formData = new URLSearchParams();
    formData.append('oldPassword', oldPassword);
    formData.append('newPassword', newPassword);
    // 发送修改密码请求
    $.ajax({
        type: 'PATCH',
        url: '/update_password',  // 假设后端接口为/update_password
        data: formData.toString(),
        contentType: 'application/x-www-form-urlencoded',
        success: function(response, textStatus, xhr) {
            if (xhr.status === 200) {
                alert('密码修改成功！');
                updatePageContent("settings");
            } else {
                alert('密码修改失败！');
            }
        },
        error: function(xhr, status, error) {
            console.error('修改密码失败:', status, error);
            alert('密码修改失败！');
        }
    });
}

// 消息中心
// 消息数据模型
const MessageModel = {
    // 消息类型映射，用于处理服务器返回的不同类型的消息
    TYPE_MAP: {
        'system': '系统通知',
        'comment': '评论通知',
        'like': '点赞通知',
        'follow': '关注通知'
    },

    // 消息数据
    messages: [],

    // 新增获取消息的方法
    fetchMessages() {
        fetch('/messages', {
            method: 'GET',
            headers: {
                'Content-Type': 'application/json',
            }
        })
        .then(response => {
            if (!response.ok) {
                throw new Error('获取消息失败');
            }
            return response.json();
        })
        .then(data => {
            // 处理服务器返回的消息数据
            this.messages = data.messages.map(msg => ({
                id: msg.id,
                type: this.TYPE_MAP[msg.type] || msg.type,
                content: msg.content,
                time: msg.postTime,
                isRead: msg.isRead === '1', // 服务器返回的是字符串'0'或'1'
                relatedLink: msg.relatedLink
            }));
            this.updateMessageDisplay();
        })
        .catch(error => {
            console.error('获取消息错误:', error);
            alert('无法获取消息，请稍后重试');
        });
    },

    // 获取未读消息数量
    getUnreadCount() {
        return this.messages.filter(msg => !msg.isRead).length;
    },

    // 标记消息为已读
    markAsRead(messageId) {
        const message = this.messages.find(msg => msg.id === messageId);
        if (message) {
            // 发送标记已读的请求到服务器
            fetch(`/mark_message_read?messageId=${messageId}`, {
                method: 'PATCH',
                headers: {
                    'Content-Type': 'application/json',
                }
            })
            .then(response => {
                if (response.ok) {
                    message.isRead = true;
                    this.updateMessageDisplay();
                } else {
                    throw new Error('标记消息失败');
                }
            })
            .catch(error => {
                console.error('标记消息已读失败:', error);
                alert('标记消息失败，请重试');
            });
        }
    },

    // 标记所有消息为已读
    markAllAsRead() {
        fetch('/mark_all_messages_read', {
            method: 'PATCH',
            headers: {
                'Content-Type': 'application/json',
            }
        })
        .then(response => {
            if (response.ok) {
                this.messages.forEach(msg => msg.isRead = true);
                this.updateMessageDisplay();
            } else {
                throw new Error('标记所有消息失败');
            }
        })
        .catch(error => {
            console.error('标记所有消息已读失败:', error);
            alert('标记消息失败，请重试');
        });
    },

    // 删除消息
    deleteMessage(messageId) {
        fetch(`/delete_message?messageId=${messageId}`, {
            method: 'DELETE',
            headers: {
                'Content-Type': 'application/json',
            }
        })
        .then(response => {
            if (response.ok) {
                const index = this.messages.findIndex(msg => msg.id === messageId);
                if (index !== -1) {
                    this.messages.splice(index, 1);
                    this.updateMessageDisplay();
                }
            } else {
                throw new Error('删除消息失败');
            }
        })
        .catch(error => {
            console.error('删除消息失败:', error);
            alert('删除消息失败，请重试');
        });
    },

    // 更新消息显示
    updateMessageDisplay() {
        const content = document.getElementById('content');
        const unreadCount = this.getUnreadCount();
    
        content.innerHTML = `
            <div class="message-center">
                <div class="message-header">
                    <h2>
                        消息中心 
                        <span class="unread-badge">${unreadCount > 0 ? `(${unreadCount}条未读)` : ''}</span>
                    </h2>
                </div>
                <div class="message-controls">
                    <button onclick="MessageModel.markAllAsRead()" class="mark-all-read-btn">
                        全部标记为已读
                    </button>
                </div>
                <div class="message-list">
                    ${this.messages.length > 0 ? 
                        this.messages.map(msg => `
                            <div class="message-item ${msg.isRead ? 'read' : 'unread'}">
                                <div class="message-header">
                                    <h4>${msg.type}</h4>
                                    <span class="time">${msg.time}</span>
                                </div>
                                <p>${msg.content}</p>
                                <div class="message-actions">
                                    ${!msg.isRead ? `<button onclick="MessageModel.markAsRead(${msg.id})">标记已读</button>` : ''}
                                    ${msg.relatedLink ? `<a href="blog_detail_user.html?blogId=${msg.relatedLink}" class="view-link">查看详情</a>` : ''}
                                    <button onclick="MessageModel.deleteMessage(${msg.id})">删除</button>
                                </div>
                            </div>
                        `).join('') : 
                        '<p class="no-messages">暂无新消息</p>'
                    }
                </div>
            </div>
        `;
    }
};

// 在页面加载时初始化消息模型(这个可以删掉)
document.addEventListener('DOMContentLoaded', function() {
    // 如果需要，可以在这里添加额外的初始化逻辑
});