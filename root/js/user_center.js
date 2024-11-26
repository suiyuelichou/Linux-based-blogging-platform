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

function updatePageContent(page, data) {
    const content = document.getElementById('content');
    content.innerHTML = ''; // 清空旧内容

    if (page === 'profile') {
        content.innerHTML = `
            <h2>个人资料</h2>
            <div class="profile-info">
                <div class="info-item"><label>昵称：</label><span>${data.username}</span></div>
                <div class="info-item"><label>邮箱：</label><span>${data.email}</span></div>
                <div class="info-item"><label>简介：</label><p>${data.description}</p></div>
            </div>`;
    } else if (page === 'manage') {
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
    } else if (page === 'settings') {
        content.innerHTML = `
            <h2>账号设置</h2>
            <div class="settings-form">
                <div class="form-item"><label>修改密码</label><input type="password" placeholder="请输入新密码"></div>
                <div class="form-item"><label>确认密码</label><input type="password" placeholder="请确认新密码"></div>
                <button class="save-btn">保存修改</button>
            </div>`;
    } else if (page === 'messages') {
        content.innerHTML = `<h2>消息中心</h2><div class="message-list">${
            data.messages.map(msg => `
                <div class="message-item">
                    <h4>${msg.type}</h4>
                    <p>${msg.content}</p>
                    <span class="time">${msg.time}</span>
                </div>
            `).join('')
        }</div>`;
    }
}

// 编辑博客
function editBlog(blogId) {
    // 跳转到博客编辑页面，传递 blogId 参数
    window.location.href = `blog_editor_modify.html?blogId=${blogId}`;
}

// 删除博客
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
function confirmDelete(blogId) {
    fetch(`/delete_blog`, {
        method: 'DELETE',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ blogId }),
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

