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

// document.addEventListener("DOMContentLoaded", () => {
//     // loadPageData('profile'); // 默认加载个人资料页面
//     loadPageData('manage');
// });

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
